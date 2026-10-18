#pragma once

#include <string>
#include <string_view>

namespace motorforge {

struct SceneCommandResult {
    bool ok{false};
    std::string message;
    std::string json;
    std::string revision;
};

[[nodiscard]] SceneCommandResult list_scenes(std::string_view project_path);
[[nodiscard]] SceneCommandResult create_scene(
    std::string_view project_path, std::string_view name, std::string_view requested_id = {});
[[nodiscard]] SceneCommandResult inspect_scene(
    std::string_view project_path, std::string_view scene_id);
[[nodiscard]] SceneCommandResult create_entity(
    std::string_view project_path,
    std::string_view scene_id,
    std::string_view entity_id,
    std::string_view name,
    double x,
    double y,
    std::string_view sprite_asset,
    std::string_view expected_revision);
[[nodiscard]] SceneCommandResult update_entity(
    std::string_view project_path, std::string_view scene_id,
    std::string_view entity_id, std::string_view name, double x, double y,
    std::string_view expected_revision);
[[nodiscard]] SceneCommandResult delete_entity(
    std::string_view project_path, std::string_view scene_id,
    std::string_view entity_id, std::string_view expected_revision);

} // namespace motorforge
