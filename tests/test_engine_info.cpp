#include "core/engine_info.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    require(!motorforge::version().empty(), "engine version must not be empty");
    require(motorforge::contract_version() == "1", "initial contract version must be stable");

    const auto capabilities = motorforge::capabilities_json();
    require(capabilities.find("\"contract_version\": \"1\"") != std::string_view::npos,
            "capabilities must expose contract version");
    require(capabilities.find("\"name\": \"Kor Motorforge\"") != std::string_view::npos,
            "capabilities must identify the engine");
    require(capabilities.find("\"name\": \"capabilities\"") != std::string_view::npos,
            "capabilities must describe its own command");
    require(capabilities.find("\"mutates_state\": false") != std::string_view::npos,
            "initial tools must declare read-only behavior");

    std::cout << "All core contract tests passed.\n";
    return EXIT_SUCCESS;
}
