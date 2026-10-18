#include "core/engine_info.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_help() {
    std::cout
        << "Kor Motorforge " << motorforge::version() << "\n"
        << "An LLM-first, modular game engine.\n\n"
        << "Usage:\n"
        << "  motorforge --help\n"
        << "  motorforge --version\n"
        << "  motorforge capabilities --json\n\n"
        << "The CLI contract is version " << motorforge::contract_version() << ".\n";
}

void print_version() {
    std::cout
        << "Kor Motorforge " << motorforge::version()
        << " (tool contract " << motorforge::contract_version() << ")\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1 || std::string_view{argv[1]} == "--help" ||
        std::string_view{argv[1]} == "-h") {
        print_help();
        return 0;
    }

    const std::string_view command{argv[1]};

    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    if (command == "capabilities") {
        if (argc >= 3 && std::string_view{argv[2]} == "--json") {
            std::cout << motorforge::capabilities_json() << '\n';
            return 0;
        }

        std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge capabilities --json'\n";
        return 2;
    }

    std::cerr << "error[UNKNOWN_COMMAND]: unknown command '" << command
              << "'. Run 'motorforge --help'.\n";
    return 2;
}
