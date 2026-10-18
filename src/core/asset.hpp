#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct AssetImportResult {
    bool ok{false};
    std::string asset_id;
    std::filesystem::path project_root;
    std::filesystem::path source_path;
    std::filesystem::path asset_path;
    std::string format;
    int width{0};
    int height{0};
    bool has_alpha{false};
    std::string hash;
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
    std::string json;
};

struct AssetValidationResult {
    bool ok{false};
    std::string asset_id;
    std::filesystem::path project_root;
    std::filesystem::path asset_path;
    std::string format;
    int width{0};
    int height{0};
    bool has_alpha{false};
    std::string hash;
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
    std::string json;
};

[[nodiscard]] AssetImportResult import_asset(
    const std::filesystem::path& project_root,
    const std::filesystem::path& source_path,
    std::string_view requested_id = {},
    std::string_view asset_type = "image");

[[nodiscard]] AssetValidationResult validate_asset(
    const std::filesystem::path& project_root,
    std::string_view asset_id);

[[nodiscard]] std::string asset_import_result_json(
    const AssetImportResult& result);

[[nodiscard]] std::string asset_validation_result_json(
    const AssetValidationResult& result);

} // namespace motorforge
