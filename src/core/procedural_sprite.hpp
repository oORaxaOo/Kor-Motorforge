#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace motorforge {

struct SpriteGenerationResult {
    bool ok{false};
    std::filesystem::path output_path;
    std::string type;
    std::uint32_t seed{0};
    int size{64};
    std::vector<std::string> error_codes;
    std::vector<std::string> messages;
};

[[nodiscard]] std::string generate_procedural_sprite_svg(
    std::string_view type, std::uint32_t seed, int size);
[[nodiscard]] SpriteGenerationResult write_procedural_sprite(
    const std::filesystem::path& output_path,
    std::string_view type,
    std::uint32_t seed,
    int size);
[[nodiscard]] std::string sprite_generation_result_json(
    const SpriteGenerationResult& result);

} // namespace motorforge
