#include "core/project_validator.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace motorforge {
namespace {

using Json = nlohmann::json;

void add_error(ValidationResult& result, std::string code, std::string message,
               std::string location = {}) {
    result.diagnostics.push_back(
        Diagnostic{std::move(code), std::move(message), std::move(location)});
}

bool is_non_empty_string(const Json& value) {
    return value.is_string() && !value.get<std::string>().empty();
}

bool has_semver_shape(const std::string& value) {
    static const std::regex pattern{R"(^[0-9]+\.[0-9]+\.[0-9]+$)"};
    return std::regex_match(value, pattern);
}

std::filesystem::path resolve_manifest(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        return path / "project.json";
    }
    return path;
}

} // namespace

ValidationResult validate_project(const std::filesystem::path& path) {
    ValidationResult result;
    result.manifest_path = resolve_manifest(path);

    std::error_code ec;
    if (!std::filesystem::exists(result.manifest_path, ec) ||
        !std::filesystem::is_regular_file(result.manifest_path, ec)) {
        add_error(result, "PROJECT_NOT_FOUND",
                  "Project manifest was not found at the requested path.",
                  result.manifest_path.generic_string());
        return result;
    }

    std::ifstream input{result.manifest_path};
    if (!input) {
        add_error(result, "PROJECT_READ_FAILED",
                  "Project manifest exists but could not be opened for reading.",
                  result.manifest_path.generic_string());
        return result;
    }

    Json document;
    try {
        input >> document;
    } catch (const Json::parse_error& error) {
        add_error(result, "INVALID_JSON", error.what(),
                  result.manifest_path.generic_string());
        return result;
    }

    if (!document.is_object()) {
        add_error(result, "INVALID_PROJECT_ROOT",
                  "Project manifest root must be a JSON object.", "$");
        return result;
    }

    if (!document.contains("schema_version") ||
        !document["schema_version"].is_number_integer()) {
        add_error(result, "SCHEMA_VERSION_REQUIRED",
                  "schema_version must be an integer.", "$.schema_version");
    } else if (document["schema_version"].get<int>() != 1) {
        add_error(result, "UNSUPPORTED_SCHEMA_VERSION",
                  "Only project schema_version 1 is supported.",
                  "$.schema_version");
    }

    if (!document.contains("project") || !document["project"].is_object()) {
        add_error(result, "PROJECT_METADATA_REQUIRED",
                  "project must be an object.", "$.project");
    } else {
        const auto& project = document["project"];

        if (!project.contains("id") || !is_non_empty_string(project["id"])) {
            add_error(result, "PROJECT_ID_REQUIRED",
                      "project.id must be a non-empty string.", "$.project.id");
        } else {
            result.project_id = project["id"].get<std::string>();
            static const std::regex id_pattern{R"(^[a-z0-9][a-z0-9._-]*$)"};
            if (!std::regex_match(result.project_id, id_pattern)) {
                add_error(result, "INVALID_PROJECT_ID",
                          "project.id must start with a lowercase letter or digit and contain only lowercase letters, digits, '.', '_' or '-'.",
                          "$.project.id");
            }
        }

        if (!project.contains("name") || !is_non_empty_string(project["name"])) {
            add_error(result, "PROJECT_NAME_REQUIRED",
                      "project.name must be a non-empty string.", "$.project.name");
        } else {
            result.project_name = project["name"].get<std::string>();
        }
    }

    if (!document.contains("engine") || !document["engine"].is_object()) {
        add_error(result, "ENGINE_REQUIREMENTS_REQUIRED",
                  "engine must be an object.", "$.engine");
    } else {
        const auto& engine = document["engine"];
        if (!engine.contains("min_version") ||
            !is_non_empty_string(engine["min_version"])) {
            add_error(result, "ENGINE_VERSION_REQUIRED",
                      "engine.min_version must be a semantic version string.",
                      "$.engine.min_version");
        } else if (!has_semver_shape(engine["min_version"].get<std::string>())) {
            add_error(result, "INVALID_ENGINE_VERSION",
                      "engine.min_version must use MAJOR.MINOR.PATCH format.",
                      "$.engine.min_version");
        }
    }

    if (document.contains("settings") && !document["settings"].is_object()) {
        add_error(result, "INVALID_SETTINGS",
                  "settings must be an object when provided.", "$.settings");
    }

    result.ok = result.diagnostics.empty();
    return result;
}

std::string validation_result_json(const ValidationResult& result) {
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.diagnostics) {
        diagnostics.push_back({
            {"code", diagnostic.code},
            {"message", diagnostic.message},
            {"location", diagnostic.location}
        });
    }

    const Json response{
        {"contract_version", "1"},
        {"ok", result.ok},
        {"manifest_path", result.manifest_path.generic_string()},
        {"project", {
            {"id", result.project_id},
            {"name", result.project_name}
        }},
        {"diagnostics", std::move(diagnostics)}
    };
    return response.dump(2);
}

} // namespace motorforge
