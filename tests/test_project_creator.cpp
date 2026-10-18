#include "core/project_creator.hpp"
#include "core/project_validator.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-project-creator-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    const auto destination = root / "First Game";
    const auto created = motorforge::create_project(destination, "First Game");
    require(created.ok, "project creation should succeed for a new destination");
    require(created.project_id == "first-game", "project ID should be generated deterministically");

    const auto validated = motorforge::validate_project(destination);
    require(validated.ok, "created project must pass project validation");
    require(validated.project_name == "First Game", "created project must preserve display name");

    const auto collision = motorforge::create_project(destination, "Overwrite Attempt");
    require(!collision.ok, "project creation must refuse an existing destination");
    require(!collision.error_codes.empty() &&
                collision.error_codes.front() == "PROJECT_PATH_EXISTS",
            "existing destination must return stable error code");

    const auto invalid_id = motorforge::create_project(root / "Invalid", "Invalid", "Not Valid");
    require(!invalid_id.ok, "invalid explicit project IDs must be rejected");
    require(!std::filesystem::exists(root / "Invalid"),
            "invalid input must not create a partial project directory");

    const auto empty_name = motorforge::create_project(root / "NoName", "");
    require(!empty_name.ok, "empty project name must be rejected");

    const auto json = motorforge::create_project_result_json(created);
    require(json.find("\"contract_version\": \"1\"") != std::string::npos,
            "creation result must include contract version");
    require(json.find("\"ok\": true") != std::string::npos,
            "successful creation result must be machine-readable");

    std::filesystem::remove_all(root, ec);
    std::cout << "All project creator tests passed.\n";
    return EXIT_SUCCESS;
}
