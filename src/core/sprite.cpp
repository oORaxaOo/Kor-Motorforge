#include "core/sprite.hpp"
#include "core/asset.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace motorforge {
namespace {
using Json = nlohmann::json;

void add_error(SpriteManifestResult& result, std::string code, std::string message) {
    result.error_codes.push_back(std::move(code));
    result.messages.push_back(std::move(message));
}

std::filesystem::path manifest_path_for(const std::filesystem::path& project_root, std::string_view sprite_id) {
    return project_root / "assets" / (std::string{sprite_id} + ".sprite.json");
}

} // namespace

SpriteManifestResult slice_sprite_grid(const std::filesystem::path& project_root,
                                       std::string_view asset_id,
                                       std::string_view sprite_id,
                                       const SpriteSliceOptions& options) {
    SpriteManifestResult result;
    if (project_root.empty()) {
        add_error(result, "PROJECT_PATH_REQUIRED", "A project directory is required.");
        return result;
    }
    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(project_root, ec);
    if (ec) {
        add_error(result, "PROJECT_PATH_INVALID", "Could not resolve project path.");
        return result;
    }

    const auto registry = load_registry(root);
    const auto& assets = registry["assets"];
    const auto it = std::find_if(assets.begin(), assets.end(), [&](const Json& entry) {
        return entry.is_object() && entry.contains("id") && entry["id"].get<std::string>() == std::string{asset_id};
    });
    if (it == assets.end()) {
        add_error(result, "ASSET_NOT_FOUND", "Referenced asset not found in registry.");
        return result;
    }
    const auto& manifest = *it;
    const auto relative_source = manifest.contains("source") ? manifest["source"].get<std::string>() : std::string{};
    const auto source_path = root / relative_source;
    if (!std::filesystem::exists(source_path, ec)) {
        add_error(result, "ASSET_FILE_MISSING", "Referenced asset file is missing.");
        return result;
    }

    // Read image info using existing detector
    ImageInfo info{};
    if (!detect_image_info(source_path, info)) {
        add_error(result, "IMAGE_METADATA_INVALID", "Could not read image metadata.");
        return result;
    }

    if (options.frame_width <= 0 || options.frame_height <= 0) {
        add_error(result, "INVALID_FRAME_SIZE", "Frame width and height must be positive.");
        return result;
    }

    int columns = options.columns;
    int rows = options.rows;
    if (columns <= 0) {
        columns = (info.width - 2 * options.margin + options.spacing) / (options.frame_width + options.spacing);
    }
    if (rows <= 0) {
        rows = (info.height - 2 * options.margin + options.spacing) / (options.frame_height + options.spacing);
    }
    if (columns <= 0 || rows <= 0) {
        add_error(result, "GRID_COMPUTATION_FAILED", "Could not compute grid columns/rows with given frame size and image dimensions.");
        return result;
    }

    // Generate frame coordinates
    std::vector<std::pair<int,int>> frames;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            int x = options.margin + c * (options.frame_width + options.spacing);
            int y = options.margin + r * (options.frame_height + options.spacing);
            if (x + options.frame_width > info.width || y + options.frame_height > info.height) {
                // skip frames that overflow
                continue;
            }
            frames.emplace_back(x, y);
        }
    }
    if (frames.empty()) {
        add_error(result, "NO_FRAMES", "No frames were extracted with the provided grid parameters.");
        return result;
    }

    // Build manifest JSON
    Json sprite_manifest;
    sprite_manifest["schema_version"] = 1;
    sprite_manifest["id"] = std::string(sprite_id.empty() ? asset_id : sprite_id);
    sprite_manifest["source_asset"] = std::string{asset_id};
    sprite_manifest["frame_size"] = Json{{"width", options.frame_width}, {"height", options.frame_height}};
    sprite_manifest["frames"] = Json::array();
    for (const auto& p : frames) {
        sprite_manifest["frames"].push_back(Json{{"x", p.first}, {"y", p.second}});
    }
    sprite_manifest["generated_at"] = current_utc_timestamp();

    const auto manifest_path = manifest_path_for(root, sprite_manifest["id"].get<std::string>());
    std::ofstream out(manifest_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        add_error(result, "MANIFEST_WRITE_FAILED", "Could not write sprite manifest.");
        return result;
    }
    out << sprite_manifest.dump(2) << '\n';
    out.close();

    result.ok = true;
    result.sprite_id = sprite_manifest["id"].get<std::string>();
    result.manifest_path = manifest_path;
    result.frames = frames;
    result.frame_width = options.frame_width;
    result.frame_height = options.frame_height;
    result.json = sprite_manifest.dump(2);
    return result;
}

} // namespace motorforge
