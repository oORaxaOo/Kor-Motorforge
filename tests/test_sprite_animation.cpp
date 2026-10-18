# Simple unit tests for sprite slicing, atlas packing, and animation playback

#include "core/asset.hpp"
#include "core/sprite.hpp"
#include "core/atlas.hpp"
#include "core/animation.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string make_png_bytes(int width, int height) {
    // Create a minimal uncompressed PNG using zlib as in previous tests
    const std::size_t bpp = 4;
    std::vector<unsigned char> scanlines;
    for (int y = 0; y < height; ++y) {
        scanlines.push_back(0);
        for (int x = 0; x < width; ++x) {
            scanlines.push_back(0x00);
            scanlines.push_back(0x00);
            scanlines.push_back(0x00);
            scanlines.push_back(0xFF);
        }
    }
    // compress
    std::vector<unsigned char> compressed(compressBound(static_cast<uLong>(scanlines.size())));
    uLongf out_size = compressed.size();
    int ok = compress2(compressed.data(), &out_size, scanlines.data(), static_cast<uLong>(scanlines.size()), Z_BEST_SPEED);
    require(ok == Z_OK, "compress2 failed");
    compressed.resize(out_size);

    auto append_chunk = [&](const std::string& type, const std::vector<unsigned char>& data, std::vector<unsigned char>& out) {
        std::uint32_t len = static_cast<std::uint32_t>(data.size());
        out.push_back((len >> 24) & 0xFF);
        out.push_back((len >> 16) & 0xFF);
        out.push_back((len >> 8) & 0xFF);
        out.push_back(len & 0xFF);
        out.insert(out.end(), type.begin(), type.end());
        out.insert(out.end(), data.begin(), data.end());
        std::uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(type.data()), static_cast<uInt>(type.size()));
        if (!data.empty()) crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
        out.push_back((crc >> 24) & 0xFF);
        out.push_back((crc >> 16) & 0xFF);
        out.push_back((crc >> 8) & 0xFF);
        out.push_back(crc & 0xFF);
    };

    std::vector<unsigned char> png;
    png.insert(png.end(), {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A});
    std::vector<unsigned char> ihdr(13);
    ihdr[0] = (width >> 24) & 0xFF;
    ihdr[1] = (width >> 16) & 0xFF;
    ihdr[2] = (width >> 8) & 0xFF;
    ihdr[3] = (width) & 0xFF;
    ihdr[4] = (height >> 24) & 0xFF;
    ihdr[5] = (height >> 16) & 0xFF;
    ihdr[6] = (height >> 8) & 0xFF;
    ihdr[7] = (height) & 0xFF;
    ihdr[8] = 8; // bit depth
    ihdr[9] = 6; // color type RGBA
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    append_chunk("IHDR", ihdr, png);
    append_chunk("IDAT", compressed, png);
    append_chunk("IEND", {}, png);

    return std::string(png.begin(), png.end());
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-sprite-anim-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    // Create project manifest to satisfy validators
    std::ofstream manifest(root / "project.json");
    manifest << "{\"schema_version\":1,\"project\":{\"id\":\"test\",\"name\":\"Test\"},\"engine\":{\"min_version\":\"0.1.0\"}}\n";
    manifest.close();

    // create source image
    const auto source = root / "source.png";
    std::ofstream out(source, std::ios::binary | std::ios::trunc);
    out << make_png_bytes(8, 4);
    out.close();

    auto imported = motorforge::import_asset(root, source, "unit-sprite", "image");
    require(imported.ok, "import asset should succeed");

    SpriteSliceOptions opts;
    opts.frame_width = 2;
    opts.frame_height = 2;
    opts.margin = 0;
    opts.spacing = 0;
    auto sliced = motorforge::slice_sprite_grid(root, "unit-sprite", "unit-sprite", opts);
    require(sliced.ok, "sprite slicing should succeed");
    require(!sliced.frames.empty(), "sliced frames should be non-empty");

    auto atlas = motorforge::pack_sprite_into_atlas(root, "unit-sprite", "unit-sprite-atlas");
    require(atlas.ok, "atlas packing should succeed");

    // build animation
    std::vector<motorforge::AnimationFrame> frames;
    int px = 0;
    for (std::size_t i = 0; i < sliced.frames.size(); ++i) {
        motorforge::AnimationFrame f; f.x = px; f.y = 0; f.width = sliced.frame_width; f.height = sliced.frame_height; f.duration_ms = 100; if (i==1) f.event = "kick";
        frames.push_back(f);
        px += sliced.frame_width;
    }
    auto anim = motorforge::create_animation_from_frames(root, "walk", "unit-sprite-atlas", frames, 10, true);
    require(anim.ok, "animation creation should succeed");

    auto v = motorforge::validate_animation(root, "walk");
    require(v.ok, "animation should validate against atlas");

    int state = 0;
    auto step1 = motorforge::step_animation(root, "walk", 50, state);
    require(step1.ok, "animation step should succeed");
    require(step1.current_frame_index == 0, "after 50ms still frame 0");

    auto step2 = motorforge::step_animation(root, "walk", 60, state);
    require(step2.ok, "animation step 2 should succeed");
    // should have advanced to next frame and fired event 'kick'
    require((step2.current_frame_index == 1), "should be on frame 1");
    require(!step2.events.empty() && step2.events.front() == "kick", "event should have fired");

    // Non-looping animation test
    auto anim2 = motorforge::create_animation_from_frames(root, "blast", "unit-sprite-atlas", frames, 10, false);
    require(anim2.ok, "non-looping animation create");
    int s2 = 0;
    auto a1 = motorforge::step_animation(root, "blast", 5000, s2);
    require(a1.ok && a1.finished, "non-looping should finish and report finished=true");

    std::filesystem::remove_all(root, ec);
    std::cout << "All sprite and animation tests passed.\n";
    return EXIT_SUCCESS;
}
