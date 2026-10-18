#include "core/project_creator.hpp"
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

Json parse(const motorforge::SceneCommandResult& result) {
    return Json::parse(result.json);
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "motorforge-scene-store-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    const auto project = root / "game";
    const auto created_project = motorforge::create_project(project, "Scene Store Test");
    require(created_project.ok, "test project should be created");
    std::filesystem::create_directories(project / "assets");
    std::ofstream sprite_file{project / "assets" / "factory.svg"};
    sprite_file << "<svg></svg>\\n";
    sprite_file.close();
    require(static_cast<bool>(sprite_file), "test sprite asset should be writable");

    const auto empty_list = motorforge::list_scenes(project.string());
    require(empty_list.ok, "listing scenes before any are created should succeed");
    require(parse(empty_list)["result"]["scenes"].empty(), "new project should have no scenes");

    const auto created_scene = motorforge::create_scene(project.string(), "Main Scene");
    require(created_scene.ok, "scene creation should succeed");
    const auto scene_payload = parse(created_scene);
    require(scene_payload["result"]["scene"]["id"] == "main-scene",
            "scene ID should be generated from its name");
    const std::string first_revision = scene_payload["revision"].get<std::string>();

    const auto inspected = motorforge::inspect_scene(project.string(), "main-scene");
    require(inspected.ok, "created scene should be inspectable");
    require(inspected.revision == first_revision, "inspection revision should match creation revision");

    const auto created_entity = motorforge::create_entity(
        project.string(), "main-scene", "factory", "Factory", 12.5, 7.0,
        "assets/factory.svg", inspected.revision);
    require(created_entity.ok, "entity creation should succeed with current revision");
    const auto entity_payload = parse(created_entity);
    require(entity_payload["result"]["entity"]["components"]["transform"]["x"] == 12.5,
            "entity transform should preserve x coordinate");
    require(entity_payload["result"]["entity"]["components"]["sprite"]["asset"] == "assets/factory.svg",
            "sprite component should reference its asset");
    require(entity_payload["result"]["entity_count"] == 1,
            "scene entity count should increase");

    const auto stale_write = motorforge::create_entity(
        project.string(), "main-scene", "another-factory", "Stale Factory", 0.0, 0.0,
        "", inspected.revision);
    require(!stale_write.ok, "stale scene revision must reject mutation");
    require(parse(stale_write)["errors"][0]["code"] == "REVISION_CONFLICT",
            "stale mutation must return revision conflict code");

    const auto invalid_asset = motorforge::create_entity(
        project.string(), "main-scene", "bad-asset", "Bad Asset", 0.0, 0.0,
        "../outside.svg", created_entity.revision);
    require(!invalid_asset.ok, "asset paths escaping the project must be rejected");
    require(parse(invalid_asset)["errors"][0]["code"] == "INVALID_ASSET_PATH",
            "asset traversal must return stable error code");

    const auto listed = motorforge::list_scenes(project.string());
    require(listed.ok, "scene list should remain valid after entity creation");
    require(parse(listed)["result"]["scenes"].size() == 1,
            "scene list should contain the created scene");

    const auto duplicate_scene = motorforge::create_scene(project.string(), "Main Scene");
    require(!duplicate_scene.ok, "duplicate scene IDs must be rejected");
    require(parse(duplicate_scene)["errors"][0]["code"] == "SCENE_ALREADY_EXISTS",
            "duplicate scene must return stable error code");

    std::filesystem::remove_all(root, ec);
    std::cout << "All scene store tests passed.\n";
    return EXIT_SUCCESS;
}
