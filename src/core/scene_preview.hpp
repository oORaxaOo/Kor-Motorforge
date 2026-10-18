#pragma once

#include <string>
#include <string_view>

namespace motorforge {

struct ScenePreviewResult {
    bool ok{false};
    std::string message;
    std::string json;
};

[[nodiscard]] ScenePreviewResult render_scene_preview_svg(
    std::string_view project_path,
    std::string_view scene_id,
    std::string_view output_path,
    int width = 1280,
    int height = 720);

} // namespace motorforge
