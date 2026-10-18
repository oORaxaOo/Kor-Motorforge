#include "core/runtime.hpp"

#include "core/project_validator.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace motorforge {
namespace {
using Json = nlohmann::json;

RuntimeResult failure(std::string code, std::string message, std::string path = {}) {
    Json response{{"contract_version", "1"}, {"ok", false},
                  {"errors", Json::array({Json{{"code", code}, {"message", message}}})}};
    if (!path.empty()) response["path"] = path;
    return {false, std::move(message), response.dump(2)};
}

bool valid_id(std::string_view id) {
    if (id.empty() || !((id.front() >= 'a' && id.front() <= 'z') ||
                        (id.front() >= '0' && id.front() <= '9'))) return false;
    for (char ch : id) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
              ch == '.' || ch == '_' || ch == '-')) return false;
    }
    return true;
}

bool read_file(const std::filesystem::path& path, std::string& content) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) return false;
    content = buffer.str();
    return true;
}

bool inside_directory(const std::filesystem::path& root,
                      const std::filesystem::path& candidate) {
    const auto relative = candidate.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}
} // namespace

RuntimeResult inspect_runtime(std::string_view project_path, std::string_view scene_id) {
    const std::filesystem::path project{project_path};
    const auto validation = validate_project(project);
    if (!validation.ok) {
        const auto code = validation.diagnostics.empty() ? "INVALID_PROJECT"
                                                         : validation.diagnostics.front().code;
        const auto message = validation.diagnostics.empty() ? "Project validation failed."
                                                            : validation.diagnostics.front().message;
        return failure(code, message, validation.manifest_path.generic_string());
    }
    if (!valid_id(scene_id)) return failure("INVALID_SCENE_ID", "Scene ID format is invalid.");

    std::error_code ec;
    const auto root = std::filesystem::canonical(project, ec);
    if (ec) return failure("PROJECT_PATH_RESOLUTION_FAILED", "Could not resolve project directory.");
    const auto scene_path = root / "scenes" / (std::string{scene_id} + ".scene.json");
    std::string content;
    if (!read_file(scene_path, content))
        return failure("SCENE_NOT_FOUND", "Could not load the requested scene.", scene_path.generic_string());

    Json document;
    try {
        document = Json::parse(content);
    } catch (const Json::exception& error) {
        return failure("INVALID_SCENE_JSON", error.what(), scene_path.generic_string());
    }
    if (!document.is_object() || !document.contains("schema_version") ||
        !document["schema_version"].is_number_integer() || document["schema_version"].get<int>() != 1 ||
        !document.contains("scene") || !document["scene"].is_object() ||
        !document["scene"].contains("id") || !document["scene"]["id"].is_string() ||
        document["scene"]["id"].get<std::string>() != scene_id ||
        !document["scene"].contains("name") || !document["scene"]["name"].is_string() ||
        !document.contains("entities") || !document["entities"].is_array()) {
        return failure("INVALID_SCENE_SCHEMA",
            "Scene must contain schema_version 1, matching scene metadata, and an entities array.",
            scene_path.generic_string());
    }

    Json entities = Json::array();
    Json assets = Json::array();
    std::vector<std::string> seen;
    std::size_t renderable_count = 0;
    for (const auto& entity : document["entities"]) {
        if (!entity.is_object() || !entity.contains("id") || !entity["id"].is_string() ||
            !entity.contains("name") || !entity["name"].is_string() ||
            entity["name"].get<std::string>().empty() ||
            !entity.contains("components") || !entity["components"].is_object()) {
            return failure("INVALID_ENTITY", "Each runtime entity requires id, name, and components.",
                           scene_path.generic_string());
        }
        const std::string id = entity["id"].get<std::string>();
        if (!valid_id(id)) return failure("INVALID_ENTITY_ID", "Entity ID format is invalid.");
        for (const auto& previous : seen) {
            if (previous == id) return failure("DUPLICATE_ENTITY_ID", "Duplicate entity ID: " + id);
        }
        seen.push_back(id);

        const auto& components = entity["components"];
        if (!components.contains("transform") || !components["transform"].is_object())
            return failure("ENTITY_TRANSFORM_MISSING", "Entity '" + id + "' requires a transform component.");
        const auto& transform = components["transform"];
        for (const char* key : {"x", "y", "rotation", "scale_x", "scale_y"}) {
            if (!transform.contains(key) || !transform[key].is_number() ||
                !std::isfinite(transform[key].get<double>())) {
                return failure("INVALID_TRANSFORM", "Entity '" + id + "' has a missing or non-finite transform field: " + key);
            }
        }

        Json runtime_entity{{"id", id}, {"name", entity["name"]},
            {"transform", {{"x", transform["x"]}, {"y", transform["y"]},
                {"rotation", transform["rotation"]}, {"scale_x", transform["scale_x"]},
                {"scale_y", transform["scale_y"]}}}, {"has_sprite", false}};

        if (components.contains("sprite")) {
            const auto& sprite = components["sprite"];
            if (!sprite.is_object() || !sprite.contains("asset") || !sprite["asset"].is_string() ||
                sprite["asset"].get<std::string>().empty())
                return failure("INVALID_SPRITE_COMPONENT", "Entity '" + id + "' has an invalid sprite component.");
            const std::filesystem::path relative{sprite["asset"].get<std::string>()};
            if (relative.is_absolute())
                return failure("INVALID_ASSET_PATH", "Sprite paths must be project-relative.");
            const auto resolved = std::filesystem::canonical(root / relative, ec);
            if (ec || !inside_directory(root, resolved) ||
                !std::filesystem::is_regular_file(resolved, ec) || ec)
                return failure("ASSET_NOT_FOUND_OR_UNSAFE",
                    "Sprite asset for entity '" + id + "' is missing or resolves outside the project.",
                    (root / relative).generic_string());
            if (resolved.extension() != ".svg")
                return failure("UNSUPPORTED_SPRITE_FORMAT", "Headless runtime currently accepts SVG sprites only.",
                               resolved.generic_string());
            runtime_entity["has_sprite"] = true;
            runtime_entity["sprite"] = {{"asset", relative.generic_string()}, {"format", "svg"}};
            assets.push_back({{"path", relative.generic_string()}, {"format", "svg"}});
            ++renderable_count;
        }
        entities.push_back(std::move(runtime_entity));
    }

    Json response{{"contract_version", "1"}, {"ok", true},
        {"result", {{"runtime", {{"status", "ready"}, {"mode", "headless"},
                                {"renderer", "none"}, {"simulation_tick", 0}}},
            {"scene", document["scene"]}, {"entity_count", entities.size()},
            {"sprite_entity_count", renderable_count}, {"entities", std::move(entities)},
            {"assets", std::move(assets)}}}};
    return {true, "Scene loaded and validated by the headless runtime.", response.dump(2)};
}
} // namespace motorforge
