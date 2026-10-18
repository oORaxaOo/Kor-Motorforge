#include "core/asset.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using Json = nlohmann::json;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string make_png_bytes(int width, int height) {
    const std::size_t bytes_per_pixel = 4;
    const std::size_t stride = static_cast<std::size_t>(width) * bytes_per_pixel;
    std::vector<unsigned char> scanlines;
    scanlines.reserve(static_cast<std::size_t>(height) * (stride + 1));
    for (int y = 0; y < height; ++y) {
        scanlines.push_back(0);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * width + x) * bytes_per_pixel;
            (void)index;
            scanlines.push_back(0x00);
            scanlines.push_back(0x00);
            scanlines.push_back(0x00);
            scanlines.push_back(0xFF);
        }
    }

    std::vector<unsigned char> compressed;
    const std::size_t bound = compressBound(static_cast<uLong>(scanlines.size()));
    compressed.resize(bound);
    uLongf compressed_size = static_cast<uLongf>(compressed.size());
    require(compress2(compressed.data(), &compressed_size,
                      scanlines.data(), static_cast<uLong>(scanlines.size()), Z_BEST_SPEED) == Z_OK,
            "PNG compression should succeed");
    compressed.resize(static_cast<std::size_t>(compressed_size));

    auto append_chunk = [&](const std::string& type, const std::vector<unsigned char>& data) {
        std::vector<unsigned char> chunk;
        const std::uint32_t length = static_cast<std::uint32_t>(data.size());
        chunk.push_back(static_cast<unsigned char>((length >> 24) & 0xFF));
        chunk.push_back(static_cast<unsigned char>((length >> 16) & 0xFF));
        chunk.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        chunk.push_back(static_cast<unsigned char>(length & 0xFF));
        chunk.insert(chunk.end(), type.begin(), type.end());
        chunk.insert(chunk.end(), data.begin(), data.end());
        std::uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
        chunk.push_back(static_cast<unsigned char>((crc >> 24) & 0xFF));
        chunk.push_back(static_cast<unsigned char>((crc >> 16) & 0xFF));
        chunk.push_back(static_cast<unsigned char>((crc >> 8) & 0xFF));
        chunk.push_back(static_cast<unsigned char>(crc & 0xFF));
        return chunk;
    };

    std::vector<unsigned char> png;
    png.insert(png.end(), {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    std::array<unsigned char, 13> ihdr{{0, 0, 0, 0}};
    // width and height
    ihdr[0] = static_cast<unsigned char>((width >> 24) & 0xFF);
    ihdr[1] = static_cast<unsigned char>((width >> 16) & 0xFF);
    ihdr[2] = static_cast<unsigned char>((width >> 8) & 0xFF);
    ihdr[3] = static_cast<unsigned char>(width & 0xFF);
    ihdr[4] = static_cast<unsigned char>((height >> 24) & 0xFF);
    ihdr[5] = static_cast<unsigned char>((height >> 16) & 0xFF);
    ihdr[6] = static_cast<unsigned char>((height >> 8) & 0xFF);
    ihdr[7] = static_cast<unsigned char>(height & 0xFF);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    png.insert(png.end(), append_chunk("IHDR", std::vector<unsigned char>(ihdr.begin(), ihdr.end())).begin(),
                append_chunk("IHDR", std::vector<unsigned char>(ihdr.begin(), ihdr.end())).end());
    png.insert(png.end(), append_chunk("IDAT", compressed).begin(), append_chunk("IDAT", compressed).end());
    png.insert(png.end(), append_chunk("IEND", {}).begin(), append_chunk("IEND", {}).end());
    return std::string(png.begin(), png.end());
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-asset-pipeline-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    const auto source = root / "test.png";
    std::ofstream output(source, std::ios::binary | std::ios::trunc);
    output << make_png_bytes(2, 2);
    output.close();

    const auto imported = motorforge::import_asset(root, source, "tank-body", "image");
    require(imported.ok, "PNG import should succeed");
    require(imported.format == "png", "imported format should be png");
    require(imported.width == 2 && imported.height == 2, "PNG dimensions should be decoded");
    require(imported.has_alpha, "PNG should report alpha support");
    require(motorforge::validate_asset(root, "tank-body").ok, "imported asset should validate");

    const auto invalid = motorforge::import_asset(root, root / "missing.png", "bad");
    require(!invalid.ok && invalid.error_codes.front() == "SOURCE_FILE_NOT_FOUND",
            "missing source should fail with stable code");

    const auto outside_root = std::filesystem::temp_directory_path() / "motorforge-outside.png";
    std::ofstream outside(outside_root, std::ios::binary | std::ios::trunc);
    outside << make_png_bytes(4, 4);
    outside.close();
    const auto unsafe = motorforge::import_asset(root, outside_root, "unsafe", "image");
    require(!unsafe.ok && unsafe.error_codes.front() == "SOURCE_PATH_OUTSIDE_PROJECT",
            "external file import should be refused");

    std::filesystem::remove_all(root, ec);
    std::filesystem::remove(outside_root, ec);
    std::cout << "All asset pipeline tests passed.\n";
    return EXIT_SUCCESS;
}
