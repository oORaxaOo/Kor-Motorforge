#include "core/scene_store.hpp"

#include "core/project_creator.hpp"
#include "core/project_validator.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace motorforge {
namespace {

using Json = nlohmann::json;
constexpr std::string_view kSceneSuffix{".scene.json"};

SceneCommandResult failure(std::string code, std::string message,
                           std::string path = {}) {
    Json errors = Json::array();
    errors.push_back({{"code", code}, {"message", message}});
    Json response{
        {"contract_version", "1"},
        {"ok", false},
        {"errors", std::move(errors)}
    };
    if (!path.empty()) {
        response["path"] = path;
    }
    return {false, std::move(message), response.dump(2), {}};
}

SceneCommandResult success(std::string message, Json payload, std::string revision = {}) {
    Json response{
        {"contract_version", "1"},
        {"ok", true},
        {"result", std::move(payload)}
    };
    if (!revision.empty()) {
        response["revision"] = revision;
    }
    return {true, std::move(message), response.dump(2), std::move(revision)};
}

bool valid_id(std::string_view id) {
    if (id.empty() || !((id.front() >= 'a' && id.front() <= 'z') ||
                        (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

std::string slugify(std::string_view value) {
    std::string slug;
    bool separator = false;
    for (unsigned char character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<unsigned char>(character - 'A' + 'a');
        }
        if ((character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9')) {
            slug.push_back(static_cast<char>(character));
            separator = false;
        } else if (!slug.empty() && !separator) {
            slug.push_back('-');
            separator = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "new-scene" : slug;
}

std::string hash_revision(std::string_view content) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : content) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::filesystem::path scene_directory(const std::filesystem::path& project) {
    return project / "scenes";
}

std::filesystem::path scene_path(const std::filesystem::path& project,
                                 std::string_view scene_id) {
    return scene_directory(project) / (std::string{scene_id} + std::string{kSceneSuffix});
}

bool read_text(const std::filesystem::path& path, std::string& content,
               std::string& error_code, std::string& message) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        error_code = "SCENE_READ_FAILED";
        message = "Could not open scene file for reading.";
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        error_code = "SCENE_READ_FAILED";
        message = "Could not read complete scene file.";
        return false;
    }
    content = buffer.str();
    return true;
}

bool parse_scene(const std::string& content, Json& document,
                 std::string& error_code, std::string& message) {
    try {
        document = Json::parse(content);
    } catch (const Json::parse_error& error) {
        error_code = "INVALID_SCENE_JSON";
        message = error.what();
        return false;
    }

    if (!document.is_object() ||
        !document.contains("schema_version") ||
        !document["schema_version"].is_number_integer() ||
        document["schema_version"].get<int>() != 1) {
        error_code = "INVALID_SCENE_SCHEMA";
        message = "Scene root must be an object with schema_version 1.";
        return false;
    }
    if (!document.contains("scene") || !document["scene"].is_object() ||
        !document["scene"].contains("id") || !document["scene"]["id"].is_string() ||
        !document["scene"].contains("name") || !document["scene"]["name"].is_string()) {
        error_code = "INVALID_SCENE_METADATA";
        message = "Scene metadata must include string id and name fields.";
        return false;
    }
    if (!document.contains("entities") || !document["entities"].is_array()) {
        error_code = "INVALID_SCENE_ENTITIES";
        message = "Scene entities must be an array.";
        return false;
    }

    std::vector<std::string> ids;
    for (const auto& entity : document["entities"]) {
        if (!entity.is_object() || !entity.contains("id") || !entity["id"].is_string() ||
            !entity.contains("name") || !entity["name"].is_string() ||
            !entity.contains("components") || !entity["components"].is_object()) {
            error_code = "INVALID_ENTITY";
            message = "Each entity must have string id/name and an object components field.";
            return false;
        }
        const auto id = entity["id"].get<std::string>();
        if (!valid_id(id)) {
            error_code = "INVALID_ENTITY_ID";
            message = "Entity IDs must use lowercase letters, digits, '.', '_' or '-'.";
            return false;
        }
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
            error_code = "DUPLICATE_ENTITY_ID";
            message = "Scene contains duplicate entity IDs: " + id;
            return false;
        }
        ids.push_back(id);
    }
    return true;
}

bool atomic_write(const std::filesystem::path& path, std::string_view content,
                  std::string& error_code, std::string& message) {
    auto temporary = path;
    temporary += ".motorforge-tmp";
    std::error_code ec;
    if (std::filesystem::exists(temporary, ec)) {
        error_code = "SCENE_TEMP_PATH_EXISTS";
        message = "Temporary scene path already exists; refusing to overwrite it.";
        return false;
    }

    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        if (!output) {
            error_code = "SCENE_WRITE_FAILED";
            message = "Could not open temporary scene file for writing.";
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(temporary, ec);
            error_code = "SCENE_WRITE_FAILED";
            message = "Could not write complete scene file.";
            return false;
        }
    }

    std::filesystem::rename(temporary, path, ec);
    if (!ec) {
        return true;
    }

    // Windows does not replace an existing destination on rename. Use a backup
    // and restore it if the second rename fails; Linux remains on the atomic path above.
    ec.clear();
    if (std::filesystem::exists(path, ec)) {
        auto backup = path;
        backup += ".motorforge-backup";
        if (std::filesystem::exists(backup, ec)) {
            std::filesystem::remove(temporary, ec);
            error_code = "SCENE_BACKUP_PATH_EXISTS";
            message = "Backup path already exists; refusing to replace scene data.";
            return false;
        }
        std::filesystem::rename(path, backup, ec);
        if (ec) {
            std::filesystem::remove(temporary, ec);
            error_code = "SCENE_COMMIT_FAILED";
            message = "Could not move current scene to a temporary backup.";
            return false;
        }
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::error_code restore_error;
            std::filesystem::rename(backup, path, restore_error);
            std::filesystem::remove(temporary, restore_error);
            error_code = "SCENE_COMMIT_FAILED";
            message = "Could not finalize updated scene; the previous version was restored when possible.";
            return false;
        }
        std::filesystem::remove(backup, ec);
        return true;
    }

    std::filesystem::remove(temporary, ec);
    error_code = "SCENE_COMMIT_FAILED";
    message = "Could not finalize scene file.";
    return false;
}

bool validate_project_root(const std::filesystem::path& project,
                           SceneCommandResult& error) {
    const auto validation = validate_project(project);
    if (validation.ok) {
        return true;
    }
    error = failure(validation.diagnostics.empty() ? "INVALID_PROJECT"
                                                    : validation.diagnostics.front().code,
                    validation.diagnostics.empty() ? "Project validation failed."
                                                    : validation.diagnostics.front().message,
                    validation.manifest_path.generic_string());
    return false;
}

} // namespace

SceneCommandResult list_scenes(std::string_view project_path) {
    const std::filesystem::path project{project_path};
    SceneCommandResult project_error;
    if (!validate_project_root(project, project_error)) {
        return project_error;
    }

    const auto directory = scene_directory(project);
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        return success("No scenes have been created yet.", {{"scenes", Json::array()}});
    }

    Json scenes = Json::array();
    Json errors = Json::array();
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            return failure("SCENE_DIRECTORY_READ_FAILED",
                           "Could not enumerate the project's scenes directory.",
                           directory.generic_string());
        }
        if (!entry.is_regular_file() || !entry.path().filename().string().ends_with(kSceneSuffix)) {
            continue;
        }

        std::string content;
        std::string code;
        std::string message;
        Json document;
        if (!read_text(entry.path(), content, code, message) ||
            !parse_scene(content, document, code, message)) {
            errors.push_back({{"code", code}, {"message", message},
                              {"path", entry.path().generic_string()}});
            continue;
        }
        scenes.push_back({
            {"id", document["scene"]["id"]},
            {"name", document["scene"]["name"]},
            {"path", entry.path().generic_string()},
            {"revision", hash_revision(content)}
        });
    }

    Json response{
        {"contract_version", "1"},
        {"ok", errors.empty()},
        {"result", {{"scenes", std::move(scenes)}}},
        {"errors", std::move(errors)}
    };
    return {response["ok"].get<bool>(), "Listed project scenes.", response.dump(2), {}};
}

SceneCommandResult create_scene(std::string_view project_path,
                                std::string_view name,
                                std::string_view requested_id) {
    const std::filesystem::path project{project_path};
    SceneCommandResult project_error;
    if (!validate_project_root(project, project_error)) {
        return project_error;
    }
    if (name.empty()) {
        return failure("SCENE_NAME_REQUIRED", "A non-empty scene name is required.");
    }

    const std::string id = requested_id.empty() ? slugify(name) : std::string{requested_id};
    if (!valid_id(id)) {
        return failure("INVALID_SCENE_ID",
                       "Scene ID must start with a lowercase letter or digit and contain only lowercase letters, digits, '.', '_' or '-'.");
    }

    const auto directory = scene_directory(project);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return failure("SCENE_DIRECTORY_CREATE_FAILED",
                       "Could not create the scenes directory: " + ec.message(),
                       directory.generic_string());
    }

    const auto path = scene_path(project, id);
    if (std::filesystem::exists(path, ec)) {
        return failure("SCENE_ALREADY_EXISTS", "A scene with this ID already exists.",
                       path.generic_string());
    }

    const Json document{
        {"schema_version", 1},
        {"scene", {{"id", id}, {"name", name}}},
        {"entities", Json::array()}
    };
    const std::string serialized = document.dump(2) + "\n";
    std::string code;
    std::string message;
    if (!atomic_write(path, serialized, code, message)) {
        return failure(code, message, path.generic_string());
    }

    const auto revision = hash_revision(serialized);
    return success("Created scene " + id + ".",
                   {{"scene", document["scene"]}, {"path", path.generic_string()},
                    {"entity_count", 0}}, revision);
}

SceneCommandResult inspect_scene(std::string_view project_path,
                                 std::string_view scene_id) {
    const std::filesystem::path project{project_path};
    SceneCommandResult project_error;
    if (!validate_project_root(project, project_error)) {
        return project_error;
    }
    if (!valid_id(scene_id)) {
        return failure("INVALID_SCENE_ID", "Scene ID format is invalid.");
    }

    const auto path = scene_path(project, scene_id);
    std::string content;
    std::string code;
    std::string message;
    if (!read_text(path, content, code, message)) {
        return failure("SCENE_NOT_FOUND", "Could not find the requested scene.",
                       path.generic_string());
    }

    Json document;
    if (!parse_scene(content, document, code, message)) {
        return failure(code, message, path.generic_string());
    }
    if (document["scene"]["id"].get<std::string>() != scene_id) {
        return failure("SCENE_ID_MISMATCH",
                       "Scene ID inside the file does not match its filename.",
                       path.generic_string());
    }

    const auto revision = hash_revision(content);
    return success("Inspected scene " + std::string{scene_id} + ".",
                   {{"scene", document["scene"]}, {"entities", document["entities"]},
                    {"path", path.generic_string()}}, revision);
}

SceneCommandResult create_entity(std::string_view project_path,
                                 std::string_view scene_id,
                                 std::string_view entity_id,
                                 std::string_view name,
                                 double x, double y,
                                 std::string_view sprite_asset,
                                 std::string_view expected_revision) {
    const std::filesystem::path project{project_path};
    SceneCommandResult project_error;
    if (!validate_project_root(project, project_error)) {
        return project_error;
    }
    if (!valid_id(scene_id)) {
        return failure("INVALID_SCENE_ID", "Scene ID format is invalid.");
    }
    if (!valid_id(entity_id)) {
        return failure("INVALID_ENTITY_ID",
                       "Entity ID must start with a lowercase letter or digit and contain only lowercase letters, digits, '.', '_' or '-'.");
    }
    if (name.empty()) {
        return failure("ENTITY_NAME_REQUIRED", "A non-empty entity name is required.");
    }
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return failure("INVALID_TRANSFORM", "Entity position values must be finite numbers.");
    }
    if (expected_revision.empty()) {
        return failure("EXPECTED_REVISION_REQUIRED",
                       "Mutating a scene requires the revision returned by scene inspect.");
    }

    if (!sprite_asset.empty()) {
        const std::filesystem::path asset{sprite_asset};
        if (asset.is_absolute()) {
            return failure("INVALID_ASSET_PATH", "Sprite asset path must be relative to the project.");
        }
        for (const auto& component : asset) {
            if (component == "..") {
                return failure("INVALID_ASSET_PATH", "Sprite asset path must not escape the project directory.");
            }
        }
    }

    const auto path = scene_path(project, scene_id);
    std::string content;
    std::string code;
    std::string message;
    if (!read_text(path, content, code, message)) {
        return failure("SCENE_NOT_FOUND", "Could not find the requested scene.",
                       path.generic_string());
    }

    const auto current_revision = hash_revision(content);
    if (current_revision != expected_revision) {
        return failure("REVISION_CONFLICT",
                       "Scene changed since it was inspected. Inspect it again before retrying.",
                       path.generic_string());
    }

    Json document;
    if (!parse_scene(content, document, code, message)) {
        return failure(code, message, path.generic_string());
    }
    if (document["scene"]["id"].get<std::string>() != scene_id) {
        return failure("SCENE_ID_MISMATCH",
                       "Scene ID inside the file does not match its filename.",
                       path.generic_string());
    }

    for (const auto& entity : document["entities"]) {
        if (entity["id"].get<std::string>() == entity_id) {
            return failure("ENTITY_ALREADY_EXISTS",
                           "An entity with this ID already exists in the scene.",
                           path.generic_string());
        }
    }

    Json components{
        {"transform", {
            {"x", x}, {"y", y}, {"rotation", 0.0},
            {"scale_x", 1.0}, {"scale_y", 1.0}
        }}
    };
    if (!sprite_asset.empty()) {
        components["sprite"] = {{"asset", sprite_asset}};
    }

    const Json entity{
        {"id", entity_id},
        {"name", name},
        {"components", std::move(components)}
    };
    document["entities"].push_back(entity);
    const std::string serialized = document.dump(2) + "\n";
    if (!atomic_write(path, serialized, code, message)) {
        return failure(code, message, path.generic_string());
    }

    const auto revision = hash_revision(serialized);
    return success("Created entity " + std::string{entity_id} + ".",
                   {{"scene_id", scene_id}, {"entity", entity},
                    {"entity_count", document["entities"].size()},
                    {"path", path.generic_string()}}, revision);
}

} // namespace motorforge
