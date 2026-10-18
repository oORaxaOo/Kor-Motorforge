#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct CreateProjectResult {
    bool ok{false};
    std::filesystem::path project_path;
    std::string project_id;
    std::string project_name;
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
};

[[nodiscard]] CreateProjectResult create_project(
    const std::filesystem::path& path,
    std::string_view name,
    std::string_view requested_id = {});
[[nodiscard]] std::string create_project_result_json(const CreateProjectResult& result);

} // namespace motorforge
