#include "core/project_creator.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <utility>

namespace motorforge {
namespace {

using Json = nlohmann::json;

bool valid_id(std::string_view id) {
    if (id.empty() || !std::isalnum(static_cast<unsigned char>(id.front()))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char character) {
        const auto value = static_cast<unsigned char>(character);
        return (value >= 'a' && value <= 'z') ||
               (value >= '0' && value <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

std::string slugify(std::string_view name) {
    std::string result;
    bool previous_separator = false;

    for (const char character : name) {
        const auto value = static_cast<unsigned char>(character);
        char output = character;
        if (value >= 'A' && value <= 'Z') {
            output = static_cast<char>(value - 'A' + 'a');
        }

        const bool allowed = (output >= 'a' && output <= 'z') ||
                             (output >= '0' && output <= '9');
        if (allowed) {
            result.push_back(output);
            previous_separator = false;
        } else if (!result.empty() && !previous_separator) {
            result.push_back('-');
            previous_separator = true;
        }
    }

    while (!result.empty() && (result.back() == '-' || result.back() == '.' ||
                               result.back() == '_')) {
        result.pop_back();
    }
    if (result.empty()) {
        return "new-game";
    }
    return result;
}

CreateProjectResult failure(const std::filesystem::path& path,
                            std::string code, std::string message) {
    CreateProjectResult result;
    result.project_path = path;
    result.error_codes.push_back(std::move(code));
    result.messages.push_back(std::move(message));
    return result;
}

} // namespace

CreateProjectResult create_project(const std::filesystem::path& path,
                                   std::string_view name,
                                   std::string_view requested_id) {
    if (name.empty()) {
        return failure(path, "PROJECT_NAME_REQUIRED",
                       "A non-empty project name is required.");
    }
    if (path.empty()) {
        return failure(path, "PROJECT_PATH_REQUIRED",
                       "A destination path is required.");
    }

    const std::string id = requested_id.empty() ? slugify(name) : std::string{requested_id};
    if (!valid_id(id)) {
        return failure(path, "INVALID_PROJECT_ID",
                       "Project ID must start with a lowercase letter or digit and contain only lowercase letters, digits, '.', '_' or '-'.");
    }

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return failure(path, "PROJECT_PATH_EXISTS",
                       "Destination already exists; refusing to overwrite project data.");
    }

    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        return failure(path, "PROJECT_PARENT_CREATE_FAILED",
                       "Could not create the destination's parent directory: " + ec.message());
    }

    if (!std::filesystem::create_directory(path, ec) || ec) {
        return failure(path, "PROJECT_CREATE_FAILED",
                       "Could not create the project directory: " + ec.message());
    }

    const auto temporary_manifest = path / "project.json.tmp";
    const auto manifest = path / "project.json";
    const Json document{
        {"schema_version", 1},
        {"project", {
            {"id", id},
            {"name", name},
            {"description", "Created with Kor Motorforge."}
        }},
        {"engine", {{"min_version", "0.1.0"}}},
        {"settings", {
            {"target", "2d"},
            {"fixed_timestep_hz", 60},
            {"seed", 12345}
        }}
    };

    {
        std::ofstream output{temporary_manifest, std::ios::binary | std::ios::trunc};
        if (!output) {
            std::filesystem::remove_all(path, ec);
            return failure(path, "PROJECT_MANIFEST_WRITE_FAILED",
                           "Could not open the new project manifest for writing.");
        }
        output << document.dump(2) << '\n';
        output.close();
        if (!output) {
            std::filesystem::remove_all(path, ec);
            return failure(path, "PROJECT_MANIFEST_WRITE_FAILED",
                           "Could not write the complete project manifest.");
        }
    }

    std::filesystem::rename(temporary_manifest, manifest, ec);
    if (ec) {
        std::filesystem::remove_all(path, ec);
        return failure(path, "PROJECT_MANIFEST_COMMIT_FAILED",
                       "Could not finalize the project manifest.");
    }

    CreateProjectResult result;
    result.ok = true;
    result.project_path = path;
    result.project_id = id;
    result.project_name = std::string{name};
    return result;
}

std::string create_project_result_json(const CreateProjectResult& result) {
    Json errors = Json::array();
    for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
        errors.push_back({
            {"code", result.error_codes[index]},
            {"message", result.messages[index]}
        });
    }

    const Json response{
        {"contract_version", "1"},
        {"ok", result.ok},
        {"project_path", result.project_path.generic_string()},
        {"project", {
            {"id", result.project_id},
            {"name", result.project_name}
        }},
        {"errors", std::move(errors)}
    };
    return response.dump(2);
}

} // namespace motorforge
