#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct AnimationFrame {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    int duration_ms{0};
    std::string event;
};

struct AnimationManifestResult {
    bool ok{false};
    std::string id;
    std::filesystem::path manifest_path;
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
    std::string json;
};

struct AnimationPlayerResult {
    bool ok{false};
    int current_frame_index{0};
    int elapsed_ms{0};
    bool finished{false};
    std::vector<std::string> events;
    std::string json;
};

[[nodiscard]] AnimationManifestResult create_animation_from_frames(
    const std::filesystem::path& project_root,
    std::string_view animation_id,
    std::string_view atlas_id,
    const std::vector<AnimationFrame>& frames,
    int fps = 30,
    bool loop = true);

// Validate animation manifest against atlas metadata
[[nodiscard]] AnimationManifestResult validate_animation(const std::filesystem::path& project_root,
                                                         std::string_view animation_id);

// Play animation headlessly for dt milliseconds (can be fractional by passing ms)
[[nodiscard]] AnimationPlayerResult step_animation(const std::filesystem::path& project_root,
                                                   std::string_view animation_id,
                                                   int dt_ms,
                                                   int& out_state);

} // namespace motorforge
