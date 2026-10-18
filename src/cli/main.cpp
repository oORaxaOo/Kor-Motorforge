#include "core/engine_info.hpp"
#include "core/project_creator.hpp"
#include "core/project_validator.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_help() {
    std::cout
        << "Kor Motorforge " << motorforge::version() << "\n"
        << "An LLM-first, modular game engine.\n\n"
        << "Usage:\n"
        << "  motorforge --help\n"
        << "  motorforge --version\n"
        << "  motorforge capabilities --json\n"
        << "  motorforge validate <project-directory-or-project.json> [--json]\n"
        << "  motorforge init <new-directory> --name <name> [--id <id>] [--json]\n\n"
        << "The CLI contract is version " << motorforge::contract_version() << ".\n";
}

void print_version() {
    std::cout
        << "Kor Motorforge " << motorforge::version()
        << " (tool contract " << motorforge::contract_version() << ")\n";
}

void print_create_errors(const motorforge::CreateProjectResult& result) {
    for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
        std::cerr << "error[" << result.error_codes[index] << "]: "
                  << result.messages[index] << '\n';
    }
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

    if (command == "validate") {
        if (argc < 3) {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge validate <project-path> [--json]'\n";
            return 2;
        }

        const auto result = motorforge::validate_project(argv[2]);
        const bool json_output = argc >= 4 && std::string_view{argv[3]} == "--json";

        if (json_output) {
            std::cout << motorforge::validation_result_json(result) << '\n';
        } else if (result.ok) {
            std::cout << "Project valid: " << result.project_name
                      << " [" << result.project_id << "]\n";
        } else {
            for (const auto& diagnostic : result.diagnostics) {
                std::cerr << "error[" << diagnostic.code << "]: "
                          << diagnostic.message;
                if (!diagnostic.location.empty()) {
                    std::cerr << " (" << diagnostic.location << ')';
                }
                std::cerr << '\n';
            }
        }

        return result.ok ? 0 : 1;
    }

    if (command == "init") {
        if (argc < 3) {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge init <new-directory> --name <name> [--id <id>] [--json]'\n";
            return 2;
        }

        const std::string destination{argv[2]};
        std::string name;
        std::string id;
        bool json_output = false;

        for (int index = 3; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--json") {
                json_output = true;
            } else if (option == "--name" && index + 1 < argc) {
                name = argv[++index];
            } else if (option == "--id" && index + 1 < argc) {
                id = argv[++index];
            } else {
                std::cerr << "error[INVALID_ARGUMENT]: unknown or incomplete option '"
                          << option << "'\n";
                return 2;
            }
        }

        const auto result = motorforge::create_project(destination, name, id);
        if (json_output) {
            std::cout << motorforge::create_project_result_json(result) << '\n';
        } else if (result.ok) {
            std::cout << "Created project: " << result.project_name
                      << " [" << result.project_id << "] at "
                      << result.project_path.generic_string() << '\n';
        } else {
            print_create_errors(result);
        }

        return result.ok ? 0 : 1;
    }

    std::cerr << "error[UNKNOWN_COMMAND]: unknown command '" << command
              << "'. Run 'motorforge --help'.\n";
    return 2;
}
