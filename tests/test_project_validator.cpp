#include "core/project_validator.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void write_file(const std::filesystem::path& path, std::string_view content) {
    std::ofstream output{path};
    output << content;
    output.close();
    require(static_cast<bool>(output), "test manifest must be writable");
}

constexpr std::string_view valid_manifest = R"json({
  "schema_version": 1,
  "project": {"id": "sample-game", "name": "Sample Game"},
  "engine": {"min_version": "0.1.0"},
  "settings": {"target": "2d", "fixed_timestep_hz": 60, "seed": 42}
})json";

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-project-validator-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    const auto manifest = root / "project.json";

    auto missing = motorforge::validate_project(root / "does-not-exist");
    require(!missing.ok, "missing project must fail validation");
    require(!missing.diagnostics.empty() &&
                missing.diagnostics.front().code == "PROJECT_NOT_FOUND",
            "missing project must return a stable error code");

    write_file(manifest, valid_manifest);
    auto valid = motorforge::validate_project(root);
    require(valid.ok, "valid project manifest must pass validation");
    require(valid.project_id == "sample-game", "validator must return project ID");
    require(valid.project_name == "Sample Game", "validator must return project name");

    write_file(manifest, R"json({"schema_version": 9})json");
    auto unsupported = motorforge::validate_project(manifest);
    require(!unsupported.ok, "unsupported schema version must fail");
    bool found_version_error = false;
    for (const auto& diagnostic : unsupported.diagnostics) {
        if (diagnostic.code == "UNSUPPORTED_SCHEMA_VERSION") {
            found_version_error = true;
        }
    }
    require(found_version_error, "unsupported schema must use stable error code");

    write_file(manifest, "{ invalid json");
    auto malformed = motorforge::validate_project(manifest);
    require(!malformed.ok, "malformed JSON must fail");
    require(!malformed.diagnostics.empty() &&
                malformed.diagnostics.front().code == "INVALID_JSON",
            "malformed JSON must use stable error code");

    const auto json_result = motorforge::validation_result_json(valid);
    require(json_result.find("\"contract_version\": \"1\"") != std::string::npos,
            "validation output must include tool contract version");
    require(json_result.find("\"ok\": true") != std::string::npos,
            "successful validation output must be machine-readable");

    std::filesystem::remove_all(root, ec);
    std::cout << "All project validator tests passed.\n";
    return EXIT_SUCCESS;
}
