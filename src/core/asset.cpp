#include "core/asset.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace motorforge {
namespace {

using Json = nlohmann::json;

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void add_import_error(AssetImportResult& result, std::string code, std::string message) {
    result.error_codes.push_back(std::move(code));
    result.messages.push_back(std::move(message));
}

void add_validation_error(AssetValidationResult& result, std::string code, std::string message) {
    result.error_codes.push_back(std::move(code));
    result.messages.push_back(std::move(message));
}

std::string current_utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_MSC_VER)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

bool valid_asset_id(std::string_view id) {
    if (id.empty()) return false;
    if (!((id.front() >= 'a' && id.front() <= 'z') ||
          (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    for (const unsigned char ch : id) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '.' || ch == '_' || ch == '-') {
            continue;
        }
        return false;
    }
    return true;
}

std::string sanitize_identifier(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const unsigned char ch : text) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            result.push_back(static_cast<char>(ch));
        } else if (ch >= 'A' && ch <= 'Z') {
            result.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else if (ch == '.' || ch == '_' || ch == '-') {
            result.push_back(static_cast<char>(ch));
        } else if (!result.empty()) {
            result.push_back('-');
        }
    }
    if (result.empty()) result = "asset";
    while (!result.empty() && (result.back() == '.' || result.back() == '_' || result.back() == '-')) {
        result.pop_back();
    }
    return result.empty() ? "asset" : result;
}

std::string fnv1a_hex(const std::string& bytes) {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const unsigned char ch : bytes) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= kFnvPrime;
    }
    std::ostringstream stream;
    stream << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

std::string read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) return {};
    return buffer.str();
}

bool is_within_directory(const std::filesystem::path& root,
                         const std::filesystem::path& candidate) {
    const auto relative = candidate.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() &&
           relative.string() != ".." &&
           relative.string().find("../") != 0;
}

std::filesystem::path resolve_asset_path(const std::filesystem::path& project_root,
                                         const std::filesystem::path& source_path,
                                         std::string_view requested_id,
                                         std::string_view format) {
    const auto asset_root = project_root / "assets";
    std::string identifier = std::string{requested_id};
    if (identifier.empty()) {
        identifier = sanitize_identifier(source_path.filename().stem().string());
    }
    identifier = sanitize_identifier(identifier);
    if (!valid_asset_id(identifier)) {
        identifier = "asset";
    }
    std::string extension = std::string{format};
    if (extension.empty()) {
        extension = source_path.extension().string();
        if (!extension.empty() && extension.front() == '.') {
            extension.erase(0, 1);
        }
    }
    if (extension.empty()) {
        extension = "asset";
    }
    return asset_root / (identifier + "." + extension);
}

struct ImageInfo {
    std::string format;
    int width{0};
    int height{0};
    bool has_alpha{false};
};

bool read_png_dimensions(const std::string& bytes, ImageInfo& info) {
    if (bytes.size() < 24) return false;
    if (static_cast<unsigned char>(bytes[0]) != 0x89 || bytes[1] != 'P' || bytes[2] != 'N' || bytes[3] != 'G' ||
        bytes[4] != '\r' || bytes[5] != '\n' || bytes[6] != 0x1A || bytes[7] != '\n') {
        return false;
    }

    std::size_t index = 8;
    while (index + 8 <= bytes.size()) {
        const std::uint32_t length =
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index])) << 24) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 1])) << 16) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 2])) << 8) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 3]));
        const std::string type(bytes.begin() + index + 4, bytes.begin() + index + 8);
        index += 8;
        if (type == "IHDR") {
            if (index + 8 > bytes.size()) return false;
            const std::uint32_t width =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index])) << 24) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 1])) << 16) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 2])) << 8) |
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 3]));
            const std::uint32_t height =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 4])) << 24) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 5])) << 16) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 6])) << 8) |
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 7]));
            const std::uint8_t bit_depth = static_cast<unsigned char>(bytes[index + 8]);
            const std::uint8_t color_type = static_cast<unsigned char>(bytes[index + 9]);
            if (width == 0 || height == 0) return false;
            info.width = static_cast<int>(width);
            info.height = static_cast<int>(height);
            info.format = "png";
            info.has_alpha = (color_type == 6 || color_type == 3 || color_type == 6);
            return true;
        }
        index += static_cast<std::size_t>(length) + 4;
    }
    return false;
}

bool read_jpeg_dimensions(const std::string& bytes, ImageInfo& info) {
    if (bytes.size() < 10) return false;
    if (static_cast<unsigned char>(bytes[0]) != 0xFF || static_cast<unsigned char>(bytes[1]) != 0xD8) {
        return false;
    }
    std::size_t index = 2;
    while (index + 9 < bytes.size()) {
        if (static_cast<unsigned char>(bytes[index]) != 0xFF) {
            return false;
        }
        const unsigned char marker = static_cast<unsigned char>(bytes[index + 1]);
        if (marker == 0xDA) {
            return false;
        }
        if (marker == 0xD9 || marker == 0xD8) {
            ++index;
            continue;
        }
        if (index + 4 > bytes.size()) return false;
        const std::uint16_t segment_length =
            (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[index + 2])) << 8) |
            static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[index + 3]));
        if (segment_length < 2) return false;
        const std::size_t payload_offset = index + 4;
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            if (payload_offset + 7 > bytes.size()) return false;
            const std::uint16_t height =
                (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[payload_offset + 1])) << 8) |
                static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[payload_offset + 2]));
            const std::uint16_t width =
                (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[payload_offset + 3])) << 8) |
                static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[payload_offset + 4]));
            info.format = "jpeg";
            info.width = static_cast<int>(width);
            info.height = static_cast<int>(height);
            info.has_alpha = false;
            return width > 0 && height > 0;
        }
        index += static_cast<std::size_t>(segment_length) + 2;
    }
    return false;
}

bool detect_image_info(const std::filesystem::path& path, ImageInfo& info) {
    const auto bytes = read_file_bytes(path);
    if (bytes.empty()) return false;
    if (bytes.size() >= 8 && bytes[0] == static_cast<char>(0x89) && bytes[1] == 'P' &&
        bytes[2] == 'N' && bytes[3] == 'G') {
        return read_png_dimensions(bytes, info);
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xD8) {
        return read_jpeg_dimensions(bytes, info);
    }
    return false;
}

std::filesystem::path canonicalize_or_empty(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) return {};
    return canonical;
}

Json load_registry(const std::filesystem::path& project_root) {
    const auto registry_path = project_root / "assets" / "registry.json";
    if (!std::filesystem::exists(registry_path)) {
        return Json{{"schema_version", 1}, {"assets", Json::array()}};
    }
    std::ifstream input(registry_path, std::ios::binary);
    if (!input) {
        return Json{{"schema_version", 1}, {"assets", Json::array()}};
    }
    try {
        Json document;
        input >> document;
        if (document.is_object() && document.contains("assets") && document["assets"].is_array()) {
            return document;
        }
    } catch (const Json::exception&) {
    }
    return Json{{"schema_version", 1}, {"assets", Json::array()}};
}

bool save_registry(const std::filesystem::path& project_root, const Json& document) {
    const auto asset_dir = project_root / "assets";
    std::error_code ec;
    std::filesystem::create_directories(asset_dir, ec);
    if (ec) return false;
    const auto registry_path = asset_dir / "registry.json";
    const auto temp_path = asset_dir / "registry.json.tmp";
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << document.dump(2) << '\n';
    output.close();
    if (!output) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }
    std::filesystem::rename(temp_path, registry_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }
    return true;
}

} // namespace

AssetImportResult import_asset(const std::filesystem::path& project_root,
                               const std::filesystem::path& source_path,
                               std::string_view requested_id,
                               std::string_view asset_type) {
    AssetImportResult result;
    result.project_root = project_root;
    result.source_path = source_path;

    if (project_root.empty()) {
        add_import_error(result, "PROJECT_PATH_REQUIRED", "A project directory is required.");
        return result;
    }

    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(project_root, ec);
    if (ec || !std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        add_import_error(result, "PROJECT_PATH_NOT_FOUND", "Project directory does not exist.");
        return result;
    }
    result.project_root = root;

    if (source_path.empty()) {
        add_import_error(result, "SOURCE_PATH_REQUIRED", "A source image path is required.");
        return result;
    }

    const auto source = std::filesystem::weakly_canonical(source_path, ec);
    if (ec || !std::filesystem::is_regular_file(source, ec)) {
        add_import_error(result, "SOURCE_FILE_NOT_FOUND", "The source image does not exist or is not a regular file.");
        return result;
    }

    const auto project_asset_root = root / "assets";
    if (!is_within_directory(root, source) && source.lexically_relative(root).string().find("..") == 0) {
        add_import_error(result, "SOURCE_PATH_OUTSIDE_PROJECT",
                         "Asset sources must resolve inside the project directory.");
        return result;
    }

    ImageInfo image_info{};
    if (!detect_image_info(source, image_info)) {
        add_import_error(result, "UNSUPPORTED_IMAGE_FORMAT",
                         "Only PNG and JPEG images are supported by the asset import pipeline.");
        return result;
    }

    if (image_info.width <= 0 || image_info.height <= 0 ||
        image_info.width > 8192 || image_info.height > 8192) {
        add_import_error(result, "INVALID_IMAGE_DIMENSIONS",
                         "Image dimensions must be greater than zero and below the project limits.");
        return result;
    }

    const std::string requested = std::string{requested_id};
    std::string id = requested.empty() ? sanitize_identifier(source.filename().stem().string()) : sanitize_identifier(requested);
    if (!valid_asset_id(id)) {
        add_import_error(result, "ASSET_ID_INVALID",
                         "Asset IDs must begin with a lowercase letter or digit and use only lowercase letters, digits, '.', '_' or '-'.");
        return result;
    }

    const auto asset_dir = root / "assets";
    std::filesystem::create_directories(asset_dir, ec);
    if (ec) {
        add_import_error(result, "ASSET_DIRECTORY_CREATE_FAILED",
                         "Could not create the project asset directory: " + ec.message());
        return result;
    }

    const auto target_path = resolve_asset_path(root, source, id, image_info.format);
    if (std::filesystem::exists(target_path, ec)) {
        add_import_error(result, "ASSET_EXISTS",
                         "Asset already exists in the project registry: " + id);
        return result;
    }

    std::filesystem::copy_file(source, target_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec || !std::filesystem::exists(target_path, ec)) {
        add_import_error(result, "ASSET_COPY_FAILED",
                         "Could not copy the source image into the project asset directory.");
        return result;
    }

    const auto bytes = read_file_bytes(target_path);
    if (bytes.empty()) {
        std::filesystem::remove(target_path, ec);
        add_import_error(result, "ASSET_READ_FAILED",
                         "Imported asset could not be reopened after copying.");
        return result;
    }

    const auto hash = fnv1a_hex(bytes);
    Json registry = load_registry(root);
    if (!registry.is_object()) {
        registry = Json{{"schema_version", 1}, {"assets", Json::array()}};
    }
    auto& assets = registry["assets"];
    for (const auto& entry : assets) {
        if (entry.is_object() && entry.contains("id") && entry["id"].get<std::string>() == id) {
            std::filesystem::remove(target_path, ec);
            add_import_error(result, "ASSET_EXISTS",
                             "Duplicate asset ID detected in the project registry.");
            return result;
        }
    }

    const Json manifest{
        {"schema_version", 1},
        {"id", id},
        {"type", std::string{asset_type}.empty() ? "image" : std::string{asset_type}},
        {"source", std::filesystem::relative(source, root).generic_string()},
        {"format", image_info.format},
        {"width", image_info.width},
        {"height", image_info.height},
        {"created_at", current_utc_timestamp()},
        {"provenance", {"kind", "imported"}},
        {"tags", Json::array({"imported", "asset"})},
        {"license", "unknown"},
        {"hash", hash},
        {"has_alpha", image_info.has_alpha}
    };
    assets.push_back(manifest);

    if (!save_registry(root, registry)) {
        std::filesystem::remove(target_path, ec);
        add_import_error(result, "REGISTRY_WRITE_FAILED",
                         "Could not save the asset registry.");
        return result;
    }

    result.ok = true;
    result.asset_id = id;
    result.asset_path = target_path;
    result.format = image_info.format;
    result.width = image_info.width;
    result.height = image_info.height;
    result.has_alpha = image_info.has_alpha;
    result.hash = hash;
    return result;
}

AssetValidationResult validate_asset(const std::filesystem::path& project_root,
                                    std::string_view asset_id) {
    AssetValidationResult result;
    result.project_root = project_root;
    result.asset_id = std::string{asset_id};

    if (project_root.empty()) {
        add_validation_error(result, "PROJECT_PATH_REQUIRED", "A project directory is required.");
        return result;
    }

    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(project_root, ec);
    if (ec || !std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        add_validation_error(result, "PROJECT_PATH_NOT_FOUND", "Project directory does not exist.");
        return result;
    }
    result.project_root = root;

    if (!valid_asset_id(asset_id)) {
        add_validation_error(result, "ASSET_ID_INVALID",
                             "Asset IDs must begin with a lowercase letter or digit and use only lowercase letters, digits, '.', '_' or '-'.");
        return result;
    }

    const auto registry = load_registry(root);
    if (!registry.contains("assets") || !registry["assets"].is_array()) {
        add_validation_error(result, "ASSET_NOT_FOUND", "The project asset registry is missing.");
        return result;
    }

    const auto& assets = registry["assets"];
    const auto it = std::find_if(assets.begin(), assets.end(), [&](const Json& entry) {
        return entry.is_object() && entry.contains("id") && entry["id"].get<std::string>() == std::string{asset_id};
    });
    if (it == assets.end()) {
        add_validation_error(result, "ASSET_NOT_FOUND", "Asset was not found in the project registry.");
        return result;
    }
    const auto& manifest = *it;
    const auto relative_path = manifest.contains("source") && manifest["source"].is_string()
        ? std::filesystem::path{manifest["source"].get<std::string>()}
        : std::filesystem::path{};
    const auto asset_path = root / relative_path;
    result.asset_path = asset_path;

    if (!std::filesystem::exists(asset_path, ec) || !std::filesystem::is_regular_file(asset_path, ec)) {
        add_validation_error(result, "ASSET_FILE_MISSING",
                             "Asset file is missing from the project asset directory.");
        return result;
    }

    ImageInfo image_info{};
    if (!detect_image_info(asset_path, image_info)) {
        add_validation_error(result, "IMAGE_METADATA_INVALID",
                             "Asset does not contain valid PNG or JPEG image metadata.");
        return result;
    }

    result.format = image_info.format;
    result.width = image_info.width;
    result.height = image_info.height;
    result.has_alpha = image_info.has_alpha;

    if (manifest.contains("format") && manifest["format"].is_string() &&
        manifest["format"].get<std::string>() != image_info.format) {
        add_validation_error(result, "FORMAT_MISMATCH",
                             "Registered asset format does not match the on-disk image format.");
    }

    if (manifest.contains("width") && manifest["width"].is_number_integer() &&
        manifest["width"].get<int>() != image_info.width) {
        add_validation_error(result, "DIMENSION_MISMATCH",
                             "Registered width does not match the decoded image dimensions.");
    }
    if (manifest.contains("height") && manifest["height"].is_number_integer() &&
        manifest["height"].get<int>() != image_info.height) {
        add_validation_error(result, "DIMENSION_MISMATCH",
                             "Registered height does not match the decoded image dimensions.");
    }

    const auto file_hash = fnv1a_hex(read_file_bytes(asset_path));
    if (manifest.contains("hash") && manifest["hash"].is_string() &&
        manifest["hash"].get<std::string>() != file_hash) {
        add_validation_error(result, "HASH_MISMATCH",
                             "Asset hash no longer matches the on-disk file contents.");
    }
    result.hash = file_hash;

    result.ok = result.error_codes.empty();
    return result;
}

std::string asset_import_result_json(const AssetImportResult& result) {
    Json errors = Json::array();
    for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
        errors.push_back({{"code", result.error_codes[index]}, {"message", result.messages[index]}});
    }

    Json payload{
        {"contract_version", "1"},
        {"ok", result.ok},
        {"project_root", result.project_root.generic_string()},
        {"asset", {
            {"id", result.asset_id},
            {"format", result.format},
            {"width", result.width},
            {"height", result.height},
            {"has_alpha", result.has_alpha},
            {"hash", result.hash},
            {"path", result.asset_path.generic_string()},
            {"source", result.source_path.generic_string()}
        }},
        {"errors", std::move(errors)}
    };
    return payload.dump(2);
}

std::string asset_validation_result_json(const AssetValidationResult& result) {
    Json errors = Json::array();
    for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
        errors.push_back({{"code", result.error_codes[index]}, {"message", result.messages[index]}});
    }

    Json payload{
        {"contract_version", "1"},
        {"ok", result.ok},
        {"project_root", result.project_root.generic_string()},
        {"asset", {
            {"id", result.asset_id},
            {"path", result.asset_path.generic_string()},
            {"format", result.format},
            {"width", result.width},
            {"height", result.height},
            {"has_alpha", result.has_alpha},
            {"hash", result.hash}
        }},
        {"errors", std::move(errors)}
    };
    return payload.dump(2);
}

} // namespace motorforge
