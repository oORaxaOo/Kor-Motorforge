#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct AtlasPackResult {
    bool ok{false};
    std::string atlas_id;
    std::filesystem::path atlas_path; // SVG atlas
    int width{0};
    int height{0};
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
    std::string json;
};

// Pack a sprite (manifest) into a deterministic atlas SVG that embeds the source image
[[nodiscard]] AtlasPackResult pack_sprite_into_atlas(
    const std::filesystem::path& project_root,
    std::string_view sprite_id,
    std::string_view atlas_id = {});

} // namespace motorforge
