#pragma once

#include <string>
#include <string_view>

namespace motorforge {

struct RuntimeResult {
    bool ok{false};
    std::string message;
    std::string json;
};

[[nodiscard]] RuntimeResult inspect_runtime(
    std::string_view project_path, std::string_view scene_id);

} // namespace motorforge
