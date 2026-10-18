#include "core/atlas.hpp"
#include "core/asset.hpp"
#include "core/sprite.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace motorforge {
namespace {
using Json = nlohmann::json;

std::string base64_encode(const std::string& in) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val=0, valb=-6;
    for (unsigned char c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0) {
            out.push_back(table[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if (valb>-6) out.push_back(table[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

std::filesystem::path sprite_manifest_path(const std::filesystem::path& project_root, std::string_view sprite_id) {
    return project_root / "assets" / (std::string{sprite_id} + ".sprite.json");
}

} // namespace

AtlasPackResult pack_sprite_into_atlas(const std::filesystem::path& project_root,
                                       std::string_view sprite_id,
                                       std::string_view atlas_id) {
    AtlasPackResult result;
    if (project_root.empty()) {
        result.error_codes.push_back("PROJECT_PATH_REQUIRED");
        result.messages.push_back("A project directory is required.");
        return result;
    }
    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(project_root, ec);
    if (ec) {
        result.error_codes.push_back("PROJECT_PATH_INVALID");
        result.messages.push_back("Could not resolve project path.");
        return result;
    }

    const auto manifest_p = sprite_manifest_path(root, sprite_id);
    if (!std::filesystem::exists(manifest_p, ec)) {
        result.error_codes.push_back("SPRITE_MANIFEST_NOT_FOUND");
        result.messages.push_back("Sprite manifest not found.");
        return result;
    }

    Json sprite_doc;
    {
        std::ifstream in(manifest_p, std::ios::binary);
        try { in >> sprite_doc; } catch (...) { }
    }
    if (!sprite_doc.is_object() || !sprite_doc.contains("frames")) {
        result.error_codes.push_back("SPRITE_MANIFEST_INVALID");
        result.messages.push_back("Sprite manifest is invalid.");
        return result;
    }

    const auto source_asset = sprite_doc.contains("source_asset") ? sprite_doc["source_asset"].get<std::string>() : std::string{};
    const auto registry = load_registry(root);
    const auto& assets = registry["assets"];
    const auto it = std::find_if(assets.begin(), assets.end(), [&](const Json& entry) {
        return entry.is_object() && entry.contains("id") && entry["id"].get<std::string>() == source_asset;
    });
    if (it == assets.end()) {
        result.error_codes.push_back("SOURCE_ASSET_NOT_FOUND");
        result.messages.push_back("Source asset not found in registry.");
        return result;
    }
    const auto source_rel = (*it).contains("source") ? (*it)["source"].get<std::string>() : std::string{};
    const auto source_path = root / source_rel;
    if (!std::filesystem::exists(source_path, ec)) {
        result.error_codes.push_back("SOURCE_FILE_MISSING");
        result.messages.push_back("Source asset file is missing.");
        return result;
    }

    // Read the file bytes and base64 them
    std::ifstream file(source_path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const std::string data_uri = std::string("data:image/") + (*it)["format"].get<std::string>() + ";base64," + base64_encode(bytes);

    const int frame_w = sprite_doc["frame_size"]["width"].get<int>();
    const int frame_h = sprite_doc["frame_size"]["height"].get<int>();
    const auto& frames = sprite_doc["frames"];

    // Simple packing: single row
    int atlas_w = 0;
    int atlas_h = frame_h;
    for (const auto& f : frames) {
        (void)f;
        atlas_w += frame_w;
    }
    if (atlas_w == 0) {
        result.error_codes.push_back("NO_FRAMES");
        result.messages.push_back("Sprite contains no frames to pack.");
        return result;
    }

    // Build an SVG embedding the source repeated, with clipping to frame areas
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << atlas_w << "\" height=\"" << atlas_h << "\">\n";
    int x = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto fx = frames[i]["x"].get<int>();
        const auto fy = frames[i]["y"].get<int>();
        // Use <image> with preserveAspectRatio='xMinYMin slice' and clipPath to extract region
        svg << "  <defs>\n";
        svg << "    <clipPath id=\"c" << i << "\"><rect x=\"0\" y=\"0\" width=\"" << frame_w << "\" height=\"" << frame_h << "\"/></clipPath>\n";
        svg << "  </defs>\n";
        // Place image with negative offset so that desired region appears inside clip
        svg << "  <g transform=\"translate(" << x << ",0)\">\n";
        svg << "    <g clip-path=\"url(#c" << i << ")\">\n";
        svg << "      <image href=\"" << data_uri << "\" x=\"" << -fx << "\" y=\"" << -fy << "\" width=\"" << (*it)["width"].get<int>() << "\" height=\"" << (*it)["height"].get<int>() << "\"/>");
        svg << "\n";
        svg << "    </g>\n";
        svg << "  </g>\n";
        x += frame_w;
    }
    svg << "</svg>\n";

    const std::string atlas_name = atlas_id && std::string{atlas_id}.size() ? std::string{atlas_id} : (std::string{sprite_doc["id"].get<std::string>()} + "-atlas");
    const auto atlas_path = root / "assets" / (atlas_name + ".svg");
    std::ofstream out(atlas_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.error_codes.push_back("ATLAS_WRITE_FAILED");
        result.messages.push_back("Could not write atlas file.");
        return result;
    }
    out << svg.str();
    out.close();

    // Build metadata JSON
    Json meta;
    meta["schema_version"] = 1;
    meta["id"] = atlas_name;
    meta["source_sprite"] = sprite_doc["id"];
    meta["image"] = std::filesystem::relative(atlas_path, root).generic_string();
    meta["width"] = atlas_w;
    meta["height"] = atlas_h;
    meta["frames"] = Json::array();
    int px = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        meta["frames"].push_back(Json{{"x", px}, {"y", 0}, {"width", frame_w}, {"height", frame_h}});
        px += frame_w;
    }

    const auto meta_path = root / "assets" / (atlas_name + ".atlas.json");
    std::ofstream mout(meta_path, std::ios::binary | std::ios::trunc);
    if (!mout) {
        result.error_codes.push_back("ATLAS_META_WRITE_FAILED");
        result.messages.push_back("Could not write atlas metadata.");
        return result;
    }
    mout << meta.dump(2) << '\n';
    mout.close();

    result.ok = true;
    result.atlas_id = atlas_name;
    result.atlas_path = atlas_path;
    result.width = atlas_w;
    result.height = atlas_h;
    result.json = meta.dump(2);
    return result;
}

} // namespace motorforge
