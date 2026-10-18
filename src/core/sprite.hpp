#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct SpriteSliceOptions {
    int frame_width{0};
    int frame_height{0};
    int margin{0};
    int spacing{0};
    int columns{0};
    int rows{0};
};

struct SpriteManifestResult {
    bool ok{false};
    std::string sprite_id;
    std::filesystem::path manifest_path;
    std::vector<std::pair<int,int>> frames; // x,y positions in source image
    int frame_width{0};
    int frame_height{0};
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
    std::string json;
};

[[nodiscard]] SpriteManifestResult slice_sprite_grid(
    const std::filesystem::path& project_root,
    std::string_view asset_id,
    std::string_view sprite_id,
    const SpriteSliceOptions& options);

} // namespace motorforge
