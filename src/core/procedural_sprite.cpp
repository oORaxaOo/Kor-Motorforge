#include "core/procedural_sprite.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace motorforge {
namespace {

using Json = nlohmann::json;

void add_error(SpriteGenerationResult& result, std::string code, std::string message) {
    result.error_codes.push_back(std::move(code));
    result.messages.push_back(std::move(message));
}

std::string create_svg(std::uint32_t seed, int size) {
    std::uint32_t state = seed ^ 0x9E3779B9U;
    if (state == 0) {
        state = 0xA341316CU;
    }
    auto next = [&state]() -> std::uint32_t {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        return state;
    };

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << size
        << "\" height=\"" << size << "\" viewBox=\"0 0 64 64\" "
        << "shape-rendering=\"crispEdges\">\n";
    svg << "  <!-- Kor Motorforge procedural asset; generator=industrial-building-v1; seed="
        << seed << " -->\n";
    svg << "  <g id=\"building-shadow\"><path d=\"M6 53 L15 49 H55 L61 53 L53 57 H12 Z\" fill=\"#101316\" opacity=\"0.9\"/></g>\n";
    svg << "  <g id=\"building-body\">\n";
    svg << "    <rect x=\"12\" y=\"20\" width=\"40\" height=\"31\" fill=\"#343a40\" stroke=\"#14171a\" stroke-width=\"2\"/>\n";
    svg << "    <rect x=\"10\" y=\"17\" width=\"44\" height=\"6\" fill=\"#626a70\" stroke=\"#1b1f23\" stroke-width=\"2\"/>\n";
    svg << "    <rect x=\"18\" y=\"10\" width=\"28\" height=\"8\" fill=\"#454c52\" stroke=\"#171a1e\" stroke-width=\"2\"/>\n";
    svg << "    <rect x=\"21\" y=\"7\" width=\"22\" height=\"4\" fill=\"#72797d\"/>\n";
    svg << "    <rect x=\"15\" y=\"25\" width=\"4\" height=\"22\" fill=\"#252a2f\"/>\n";
    svg << "    <rect x=\"45\" y=\"25\" width=\"4\" height=\"22\" fill=\"#252a2f\"/>\n";
    svg << "    <rect x=\"23\" y=\"31\" width=\"18\" height=\"20\" fill=\"#1a1e22\" stroke=\"#858b8e\" stroke-width=\"1\"/>\n";
    svg << "    <rect x=\"26\" y=\"34\" width=\"12\" height=\"17\" fill=\"#272d32\"/>\n";
    svg << "    <path d=\"M23 35 H41 M23 39 H41 M23 43 H41 M23 47 H41\" stroke=\"#596168\" stroke-width=\"1\"/>\n";
    svg << "    <rect x=\"24\" y=\"48\" width=\"16\" height=\"2\" fill=\"#d4a84f\"/>\n";
    svg << "    <path d=\"M25 48 L27 50 M29 48 L31 50 M33 48 L35 50 M37 48 L39 50\" stroke=\"#25292d\" stroke-width=\"1\"/>\n";
    svg << "    <rect x=\"48\" y=\"27\" width=\"2\" height=\"17\" fill=\"#858b8e\"/>\n";
    svg << "    <path d=\"M49 29 H53 V34 H49 M49 38 H53 V43 H49\" fill=\"none\" stroke=\"#8a5a3b\" stroke-width=\"1\"/>\n";
    svg << "    <path d=\"M14 23 H50\" stroke=\"#858b8e\" stroke-width=\"1\"/>\n";
    svg << "  </g>\n";

    svg << "  <g id=\"windows\">\n";
    for (int index = 0; index < 5; ++index) {
        const int x = 19 + index * 6;
        const auto variation = next() % 5U;
        const char* color = variation == 0U ? "#29343a"
                            : variation == 1U ? "#6aa0a4"
                            : variation == 2U ? "#d6b66d"
                                              : "#8f9a9d";
        svg << "    <rect x=\"" << x << "\" y=\"25\" width=\"3\" height=\"4\" fill=\"#161a1d\"/>\n";
        svg << "    <rect x=\"" << x << "\" y=\"25\" width=\"2\" height=\"3\" fill=\""
            << color << "\"/>\n";
    }
    svg << "  </g>\n";

    svg << "  <g id=\"warning-panels\">\n";
    svg << "    <rect x=\"13\" y=\"32\" width=\"6\" height=\"5\" fill=\"#a33a32\"/>\n";
    svg << "    <path d=\"M14 33 H18 M14 35 H18\" stroke=\"#e0b85b\" stroke-width=\"1\"/>\n";
    svg << "    <rect x=\"43\" y=\"44\" width=\"4\" height=\"4\" fill=\"#c19a4b\"/>\n";
    svg << "    <path d=\"M44 45 H46 M44 47 H46\" stroke=\"#25292d\" stroke-width=\"1\"/>\n";
    svg << "  </g>\n";

    svg << "  <g id=\"procedural-wear\" opacity=\"0.85\">\n";
    for (int index = 0; index < 5; ++index) {
        const int x = 14 + static_cast<int>(next() % 35U);
        const int y = 30 + static_cast<int>(next() % 18U);
        const int width = 1 + static_cast<int>(next() % 3U);
        const char* color = (next() % 2U == 0U) ? "#704936" : "#252a2e";
        svg << "    <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << width
            << "\" height=\"1\" fill=\"" << color << "\"/>\n";
    }
    svg << "  </g>\n";

    svg << "  <g id=\"antenna\">\n";
    svg << "    <path d=\"M37 7 V3 M34 4 H40\" stroke=\"#9aa1a3\" stroke-width=\"1\"/>\n";
    svg << "    <circle cx=\"37\" cy=\"3\" r=\"1\" fill=\"#a43d34\"/>\n";
    svg << "  </g>\n";
    svg << "</svg>\n";
    return svg.str();
}

} // namespace

std::string generate_procedural_sprite_svg(std::string_view type,
                                           std::uint32_t seed, int size) {
    if (type != "industrial-building" || size < 16 || size > 1024) {
        return {};
    }
    return create_svg(seed, size);
}

SpriteGenerationResult write_procedural_sprite(const std::filesystem::path& output_path,
                                               std::string_view type,
                                               std::uint32_t seed, int size) {
    SpriteGenerationResult result;
    result.output_path = output_path;
    result.type = std::string{type};
    result.seed = seed;
    result.size = size;

    if (output_path.empty()) {
        add_error(result, "OUTPUT_PATH_REQUIRED", "A destination path is required.");
        return result;
    }
    if (type != "industrial-building") {
        add_error(result, "UNSUPPORTED_SPRITE_TYPE",
                  "Supported procedural sprite type: industrial-building.");
        return result;
    }
    if (size < 16 || size > 1024) {
        add_error(result, "INVALID_SPRITE_SIZE",
                  "Sprite size must be between 16 and 1024 pixels.");
        return result;
    }
    if (output_path.extension() != ".svg") {
        add_error(result, "UNSUPPORTED_OUTPUT_FORMAT",
                  "Procedural sprites currently export as SVG files.");
        return result;
    }

    std::error_code ec;
    if (std::filesystem::exists(output_path, ec)) {
        add_error(result, "OUTPUT_PATH_EXISTS",
                  "Destination already exists; refusing to overwrite an asset.");
        return result;
    }

    const auto parent = output_path.has_parent_path()
        ? output_path.parent_path() : std::filesystem::path{"."};
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        add_error(result, "OUTPUT_DIRECTORY_CREATE_FAILED",
                  "Could not create output directory: " + ec.message());
        return result;
    }

    auto temporary_path = output_path;
    temporary_path += ".motorforge-tmp";
    if (std::filesystem::exists(temporary_path, ec)) {
        add_error(result, "OUTPUT_TEMP_PATH_EXISTS",
                  "Temporary output path already exists; refusing to overwrite it.");
        return result;
    }

    const auto svg = create_svg(seed, size);
    {
        std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
        if (!output) {
            add_error(result, "SPRITE_WRITE_FAILED",
                      "Could not open the temporary sprite file for writing.");
            return result;
        }
        output << svg;
        output.close();
        if (!output) {
            std::filesystem::remove(temporary_path, ec);
            add_error(result, "SPRITE_WRITE_FAILED",
                      "Could not write the complete sprite file.");
            return result;
        }
    }

    std::filesystem::rename(temporary_path, output_path, ec);
    if (ec) {
        std::filesystem::remove(temporary_path, ec);
        add_error(result, "SPRITE_FINALIZE_FAILED",
                  "Could not finalize the generated sprite file.");
        return result;
    }

    result.ok = true;
    return result;
}

std::string sprite_generation_result_json(const SpriteGenerationResult& result) {
    Json errors = Json::array();
    for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
        errors.push_back({
            {"code", result.error_codes[index]},
            {"message", result.messages[index]}
        });
    }

    const Json response{
        {"contract_version", "1"},
        {"ok", result.ok},
        {"output_path", result.output_path.generic_string()},
        {"asset", {
            {"type", result.type},
            {"seed", result.seed},
            {"size", result.size},
            {"format", "svg"}
        }},
        {"errors", std::move(errors)}
    };
    return response.dump(2);
}

} // namespace motorforge
