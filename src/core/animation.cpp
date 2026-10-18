#include "core/animation.hpp"
#include "core/asset.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace motorforge {
namespace {
using Json = nlohmann::json;

std::filesystem::path animation_manifest_path(const std::filesystem::path& root, std::string_view id) {
    return root / "assets" / (std::string{id} + ".animation.json");
}

std::filesystem::path atlas_meta_path(const std::filesystem::path& root, std::string_view atlas_id) {
    return root / "assets" / (std::string{atlas_id} + ".atlas.json");
}

} // namespace

AnimationManifestResult create_animation_from_frames(const std::filesystem::path& project_root,
                                                     std::string_view animation_id,
                                                     std::string_view atlas_id,
                                                     const std::vector<AnimationFrame>& frames,
                                                     int fps,
                                                     bool loop) {
    AnimationManifestResult result;
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

    const auto atlas_meta = atlas_meta_path(root, atlas_id);
    if (!std::filesystem::exists(atlas_meta, ec)) {
        result.error_codes.push_back("ATLAS_NOT_FOUND");
        result.messages.push_back("Atlas metadata not found.");
        return result;
    }
    Json atlas;
    {
        std::ifstream in(atlas_meta, std::ios::binary);
        try { in >> atlas; } catch (...) { }
    }
    if (!atlas.is_object() || !atlas.contains("frames") || !atlas.contains("width") || !atlas.contains("height")) {
        result.error_codes.push_back("ATLAS_INVALID");
        result.messages.push_back("Atlas metadata is invalid.");
        return result;
    }

    Json manifest;
    manifest["schema_version"] = 1;
    manifest["id"] = std::string(animation_id);
    manifest["atlas"] = std::string(atlas["image"].get<std::string>());
    manifest["fps"] = fps;
    manifest["loop"] = loop;
    manifest["frames"] = Json::array();
    for (const auto& f : frames) {
        Json jf;
        jf["x"] = f.x; jf["y"] = f.y; jf["width"] = f.width; jf["height"] = f.height;
        if (f.duration_ms > 0) jf["duration_ms"] = f.duration_ms;
        if (!f.event.empty()) jf["event"] = f.event;
        manifest["frames"].push_back(jf);
    }
    manifest["created_at"] = current_utc_timestamp();

    const auto path = animation_manifest_path(root, animation_id);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.error_codes.push_back("ANIMATION_WRITE_FAILED");
        result.messages.push_back("Could not write animation manifest.");
        return result;
    }
    out << manifest.dump(2) << '\n';
    out.close();

    result.ok = true;
    result.id = std::string(animation_id);
    result.manifest_path = path;
    result.json = manifest.dump(2);
    return result;
}

AnimationManifestResult validate_animation(const std::filesystem::path& project_root,
                                           std::string_view animation_id) {
    AnimationManifestResult result;
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

    const auto path = animation_manifest_path(root, animation_id);
    if (!std::filesystem::exists(path, ec)) {
        result.error_codes.push_back("ANIMATION_NOT_FOUND");
        result.messages.push_back("Animation manifest not found.");
        return result;
    }
    Json manifest;
    {
        std::ifstream in(path, std::ios::binary);
        try { in >> manifest; } catch (...) { }
    }
    if (!manifest.is_object() || !manifest.contains("frames") || !manifest.contains("atlas")) {
        result.error_codes.push_back("ANIMATION_INVALID");
        result.messages.push_back("Animation manifest is invalid.");
        return result;
    }

    const std::string atlas_rel = manifest["atlas"].get<std::string>();
    const auto atlas_path = root / atlas_rel;
    if (!std::filesystem::exists(atlas_path, ec)) {
        result.error_codes.push_back("ATLAS_IMAGE_MISSING");
        result.messages.push_back("Atlas image referenced by animation is missing.");
        return result;
    }

    // Load atlas metadata if available
    const auto atlas_meta = atlas_meta_path(root, std::filesystem::path(atlas_rel).stem().string());
    Json meta;
    if (std::filesystem::exists(atlas_meta, ec)) {
        std::ifstream a(atlas_meta, std::ios::binary);
        try { a >> meta; } catch (...) { }
    }

    // Validate frames fit into atlas if meta available
    if (meta.is_object() && meta.contains("width") && meta.contains("height")) {
        int aw = meta["width"].get<int>();
        int ah = meta["height"].get<int>();
        for (const auto& f : manifest["frames"]) {
            int x = f["x"].get<int>();
            int y = f["y"].get<int>();
            int w = f.contains("width") ? f["width"].get<int>() : 0;
            int h = f.contains("height") ? f["height"].get<int>() : 0;
            if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > aw || y + h > ah) {
                result.error_codes.push_back("FRAME_OUT_OF_BOUNDS");
                result.messages.push_back("Animation frame lies outside atlas bounds.");
            }
        }
    }

    result.ok = result.error_codes.empty();
    result.id = std::string(animation_id);
    result.manifest_path = path;
    result.json = manifest.dump(2);
    return result;
}

AnimationPlayerResult step_animation(const std::filesystem::path& project_root,
                                     std::string_view animation_id,
                                     int dt_ms,
                                     int& out_state) {
    AnimationPlayerResult result;
    if (project_root.empty()) {
        result.messages.push_back("PROJECT_PATH_REQUIRED");
        return result;
    }
    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(project_root, ec);
    if (ec) return result;

    const auto path = animation_manifest_path(root, animation_id);
    if (!std::filesystem::exists(path, ec)) return result;
    Json manifest;
    {
        std::ifstream in(path, std::ios::binary);
        try { in >> manifest; } catch (...) { }
    }
    if (!manifest.is_object() || !manifest.contains("frames")) return result;

    // Load or initialize state from out_state (stores current frame index and elapsed)
    int state = out_state; // encode as (frame_index << 16) | elapsed_ms (clamped)
    int frame_index = (state >> 16) & 0xFFFF;
    int elapsed = state & 0xFFFF;

    const auto& frames = manifest["frames"];
    if (frame_index < 0) frame_index = 0;
    if (frame_index >= static_cast<int>(frames.size())) frame_index = 0;

    // Advance time
    elapsed += dt_ms;
    bool finished = false;
    std::vector<std::string> events;
    while (true) {
        int duration = 0;
        if (frames[frame_index].contains("duration_ms")) duration = frames[frame_index]["duration_ms"].get<int>();
        else if (manifest.contains("fps")) {
            const int fps = manifest["fps"].get<int>();
            duration = std::max(1, 1000 / std::max(1, fps));
        } else {
            duration = 1000 / 30;
        }
        if (elapsed < duration) break;
        elapsed -= duration;
        // fire event if present on frame
        if (frames[frame_index].contains("event")) {
            events.push_back(frames[frame_index]["event"].get<std::string>());
        }
        frame_index += 1;
        if (frame_index >= static_cast<int>(frames.size())) {
            if (manifest.contains("loop") && manifest["loop"].get<bool>()) {
                frame_index = 0;
            } else {
                frame_index = static_cast<int>(frames.size()) - 1;
                finished = true;
                break;
            }
        }
    }

    // encode state back
    int new_state = ((frame_index & 0xFFFF) << 16) | (elapsed & 0xFFFF);
    out_state = new_state;

    result.ok = true;
    result.current_frame_index = frame_index;
    result.elapsed_ms = elapsed;
    result.finished = finished;
    result.events = std::move(events);
    return result;
}

} // namespace motorforge
