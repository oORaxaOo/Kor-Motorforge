#include "core/procedural_sprite.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

int main() {
    const auto first = motorforge::generate_procedural_sprite_svg("industrial-building", 42, 64);
    const auto same = motorforge::generate_procedural_sprite_svg("industrial-building", 42, 64);
    const auto different = motorforge::generate_procedural_sprite_svg("industrial-building", 43, 64);

    require(!first.empty(), "supported sprite type should generate SVG");
    require(first == same, "same seed must produce byte-identical SVG");
    require(first != different, "different seeds should change procedural variation");
    require(first.find("<svg ") != std::string::npos, "output must be an SVG document");
    require(first.find("generator=industrial-building-v1; seed=42") != std::string::npos,
            "output must preserve generator and seed metadata");
    require(motorforge::generate_procedural_sprite_svg("unknown", 1, 64).empty(),
            "unknown sprite types must not generate content");
    require(motorforge::generate_procedural_sprite_svg("industrial-building", 1, 8).empty(),
            "out-of-range sizes must not generate content");

    const auto root = std::filesystem::temp_directory_path() / "motorforge-procedural-sprite-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    const auto output_path = root / "factory.svg";
    const auto written = motorforge::write_procedural_sprite(output_path, "industrial-building", 42, 64);
    require(written.ok, "sprite file should be written to a new path");
    require(read_file(output_path) == first, "written asset must match deterministic generator output");

    const auto collision = motorforge::write_procedural_sprite(output_path, "industrial-building", 42, 64);
    require(!collision.ok, "sprite generator must refuse to overwrite existing files");
    require(!collision.error_codes.empty() && collision.error_codes.front() == "OUTPUT_PATH_EXISTS",
            "overwrite refusal must use a stable error code");

    const auto json = motorforge::sprite_generation_result_json(written);
    require(json.find("\"contract_version\": \"1\"") != std::string::npos,
            "generation result must include tool contract version");
    require(json.find("\"ok\": true") != std::string::npos,
            "generation result must be machine-readable");

    std::filesystem::remove_all(root, ec);
    std::cout << "All procedural sprite tests passed.\n";
    return EXIT_SUCCESS;
}
