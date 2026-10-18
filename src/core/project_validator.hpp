#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace motorforge {

struct Diagnostic {
    std::string code;
    std::string message;
    std::string location;
};

struct ValidationResult {
    bool ok{false};
    std::filesystem::path manifest_path;
    std::string project_id;
    std::string project_name;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] ValidationResult validate_project(const std::filesystem::path& path);
[[nodiscard]] std::string validation_result_json(const ValidationResult& result);

} // namespace motorforge
