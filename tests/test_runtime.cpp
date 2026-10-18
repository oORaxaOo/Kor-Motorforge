#include "core/project_creator.hpp"
#include "core/runtime.hpp"
#include "core/scene_store.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
using Json = nlohmann::json;
void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
Json parse(const motorforge::RuntimeResult& result) { return Json::parse(result.json); }
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-runtime-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    const auto project = root / "game";
    require(motorforge::create_project(project, "Runtime Test").ok, "project should be created");
    std::filesystem::create_directories(project / "assets");
    std::ofstream sprite(project / "assets" / "factory.svg");
    sprite << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\"></svg>\n";
    sprite.close();

    const auto scene = motorforge::create_scene(project.string(), "Main Scene");
    require(scene.ok, "scene should be created");
    auto inspected = motorforge::inspect_scene(project.string(), "main-scene");
    const auto entity = motorforge::create_entity(project.string(), "main-scene", "factory",
        "Factory", 12.5, 7.0, "assets/factory.svg", inspected.revision);
    require(entity.ok, "sprite entity should be created");

    const auto loaded = motorforge::inspect_runtime(project.string(), "main-scene");
    require(loaded.ok, "headless runtime should load a valid scene");
    const auto payload = parse(loaded);
    require(payload["result"]["runtime"]["status"] == "ready", "runtime should report ready");
    require(payload["result"]["runtime"]["mode"] == "headless", "runtime should be headless");
    require(payload["result"]["entity_count"] == 1, "runtime should load one entity");
    require(payload["result"]["sprite_entity_count"] == 1, "SVG sprite should be renderable");
    require(payload["result"]["entities"][0]["transform"]["x"] == 12.5, "runtime should preserve transform");
    require(payload["result"]["assets"][0]["path"] == "assets/factory.svg", "runtime should report resolved asset");

    const auto missing = motorforge::inspect_runtime(project.string(), "absent-scene");
    require(!missing.ok, "runtime should reject missing scenes");
    require(parse(missing)["errors"][0]["code"] == "SCENE_NOT_FOUND", "missing scene error should be stable");

    const auto bad_entity = motorforge::create_entity(project.string(), "main-scene", "missing-sprite",
        "Missing Sprite", 0.0, 0.0, "", entity.revision);
    require(bad_entity.ok, "entity without sprite can be stored");
    const auto bad_scene_path = project / "scenes" / "main-scene.scene.json";
    Json doc;
    {
        std::ifstream input(bad_scene_path);
        input >> doc;
    }
    doc["entities"][1]["components"]["sprite"] = {{"asset", "assets/missing.svg"}};
    {
        std::ofstream output(bad_scene_path, std::ios::trunc);
        output << doc.dump(2) << '\n';
    }
    const auto missing_asset = motorforge::inspect_runtime(project.string(), "main-scene");
    require(!missing_asset.ok, "runtime should reject missing referenced assets");
    require(parse(missing_asset)["errors"][0]["code"] == "ASSET_NOT_FOUND_OR_UNSAFE",
            "missing asset should produce a stable runtime error");

    std::filesystem::remove_all(root, ec);
    std::cout << "All headless runtime tests passed.\n";
    return EXIT_SUCCESS;
}
