#include "core/engine_info.hpp"
#include "core/project_creator.hpp"
#include "core/project_validator.hpp"
#include "core/procedural_sprite.hpp"
#include "core/scene_store.hpp"
#include "core/runtime.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

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
        << "  motorforge init <new-directory> --name <name> [--id <id>] [--json]\n"
        << "  motorforge spritegen <output.svg> [--type industrial-building] [--seed <uint32>] [--size <16..1024>] [--json]\n"
        << "  motorforge scene list <project> [--json]\n"
        << "  motorforge scene create <project> --name <name> [--id <id>] [--json]\n"
        << "  motorforge scene inspect <project> <scene-id> [--json]\n"
        << "  motorforge runtime inspect <project> <scene-id> [--json]\n"
        << "  motorforge entity create <project> <scene-id> --id <id> --name <name> --expected-revision <revision> [--x <number>] [--y <number>] [--sprite <asset>] [--json]\n"
        << "  motorforge entity update <project> <scene-id> --id <id> --name <name> --expected-revision <revision> --x <number> --y <number> [--json]\n"
        << "  motorforge entity delete <project> <scene-id> --id <id> --expected-revision <revision> [--json]\n\n"
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

template <typename Integer>
bool parse_integer(std::string_view text, Integer& value) {
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse_number(std::string_view text, double& value) {
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && std::isfinite(value);
}

int report_scene_result(const motorforge::SceneCommandResult& result, bool json_output) {
    if (json_output) {
        std::cout << result.json << '\n';
    } else if (result.ok) {
        std::cout << result.message << '\n';
        if (!result.revision.empty()) {
            std::cout << "revision: " << result.revision << '\n';
        }
    } else {
        std::cerr << "error: " << result.message << '\n';
    }
    return result.ok ? 0 : 1;
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

    if (command == "scene") {
        if (argc < 4) {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge scene <list|create|inspect> <project> ...'\n";
            return 2;
        }

        const std::string_view action{argv[2]};
        const std::string project{argv[3]};
        bool json_output = false;
        motorforge::SceneCommandResult result;

        if (action == "list") {
            for (int index = 4; index < argc; ++index) {
                if (std::string_view{argv[index]} == "--json") {
                    json_output = true;
                } else {
                    std::cerr << "error[INVALID_ARGUMENT]: unknown option '" << argv[index] << "'\n";
                    return 2;
                }
            }
            result = motorforge::list_scenes(project);
        } else if (action == "create") {
            std::string name;
            std::string id;
            for (int index = 4; index < argc; ++index) {
                const std::string_view option{argv[index]};
                if (option == "--json") {
                    json_output = true;
                } else if (option == "--name" && index + 1 < argc) {
                    name = argv[++index];
                } else if (option == "--id" && index + 1 < argc) {
                    id = argv[++index];
                } else {
                    std::cerr << "error[INVALID_ARGUMENT]: unknown or incomplete option '" << option << "'\n";
                    return 2;
                }
            }
            result = motorforge::create_scene(project, name, id);
        } else if (action == "inspect") {
            if (argc < 5) {
                std::cerr << "error[ARGUMENT_REQUIRED]: scene inspect requires a scene ID.\n";
                return 2;
            }
            const std::string scene_id{argv[4]};
            for (int index = 5; index < argc; ++index) {
                if (std::string_view{argv[index]} == "--json") {
                    json_output = true;
                } else {
                    std::cerr << "error[INVALID_ARGUMENT]: unknown option '" << argv[index] << "'\n";
                    return 2;
                }
            }
            result = motorforge::inspect_scene(project, scene_id);
        } else {
            std::cerr << "error[UNKNOWN_SCENE_ACTION]: unknown scene action '" << action << "'.\n";
            return 2;
        }
        return report_scene_result(result, json_output);
    }

    if (command == "runtime") {
        if (argc < 5 || std::string_view{argv[2]} != "inspect") {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge runtime inspect <project> <scene-id> [--json]'\n";
            return 2;
        }
        const bool json_output = argc == 6 && std::string_view{argv[5]} == "--json";
        if (argc > 6 || (argc == 6 && !json_output)) {
            std::cerr << "error[INVALID_ARGUMENT]: unexpected runtime inspect argument\n";
            return 2;
        }
        const auto result = motorforge::inspect_runtime(argv[3], argv[4]);
        return report_scene_result({result.ok, result.message, result.json, {}}, json_output);
    }

    if (command == "entity") {
        if (argc < 5) {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge entity <create|update|delete> <project> <scene-id> ...'.\n";
            return 2;
        }

        const std::string project{argv[3]};
        const std::string scene_id{argv[4]};
        const std::string_view entity_action{argv[2]};
        if (entity_action == "update" || entity_action == "delete") {
            std::string id;
            std::string name;
            std::string expected_revision;
            double x = 0.0;
            double y = 0.0;
            bool has_x = false;
            bool has_y = false;
            bool json_output = false;
            for (int index = 5; index < argc; ++index) {
                const std::string_view option{argv[index]};
                if (option == "--json") {
                    json_output = true;
                } else if (option == "--id" && index + 1 < argc) {
                    id = argv[++index];
                } else if (option == "--name" && index + 1 < argc) {
                    name = argv[++index];
                } else if (option == "--expected-revision" && index + 1 < argc) {
                    expected_revision = argv[++index];
                } else if (option == "--x" && index + 1 < argc) {
                    if (!parse_number(argv[++index], x)) {
                        std::cerr << "error[INVALID_TRANSFORM]: x must be a finite number.\n";
                        return 2;
                    }
                    has_x = true;
                } else if (option == "--y" && index + 1 < argc) {
                    if (!parse_number(argv[++index], y)) {
                        std::cerr << "error[INVALID_TRANSFORM]: y must be a finite number.\n";
                        return 2;
                    }
                    has_y = true;
                } else {
                    std::cerr << "error[INVALID_ARGUMENT]: unknown or incomplete option '" << option << "'\n";
                    return 2;
                }
            }
            if (id.empty() || expected_revision.empty()) {
                std::cerr << "error[ARGUMENT_REQUIRED]: --id and --expected-revision are required.\n";
                return 2;
            }
            if (entity_action == "update") {
                if (name.empty() || !has_x || !has_y) {
                    std::cerr << "error[ARGUMENT_REQUIRED]: entity update requires --name, --x, and --y.\n";
                    return 2;
                }
                return report_scene_result(
                    motorforge::update_entity(project, scene_id, id, name, x, y, expected_revision),
                    json_output);
            }
            if (!name.empty() || has_x || has_y) {
                std::cerr << "error[INVALID_ARGUMENT]: entity delete accepts only --id, --expected-revision, and --json.\n";
                return 2;
            }
            return report_scene_result(
                motorforge::delete_entity(project, scene_id, id, expected_revision), json_output);
        }
        if (entity_action != "create") {
            std::cerr << "error[UNKNOWN_ENTITY_ACTION]: unknown entity action '" << entity_action << "'.\n";
            return 2;
        }
        std::string id;
        std::string name;
        std::string sprite;
        std::string expected_revision;
        double x = 0.0;
        double y = 0.0;
        bool json_output = false;

        for (int index = 5; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--json") {
                json_output = true;
            } else if (option == "--id" && index + 1 < argc) {
                id = argv[++index];
            } else if (option == "--name" && index + 1 < argc) {
                name = argv[++index];
            } else if (option == "--expected-revision" && index + 1 < argc) {
                expected_revision = argv[++index];
            } else if (option == "--sprite" && index + 1 < argc) {
                sprite = argv[++index];
            } else if (option == "--x" && index + 1 < argc) {
                if (!parse_number(argv[++index], x)) {
                    std::cerr << "error[INVALID_TRANSFORM]: x must be a finite number.\n";
                    return 2;
                }
            } else if (option == "--y" && index + 1 < argc) {
                if (!parse_number(argv[++index], y)) {
                    std::cerr << "error[INVALID_TRANSFORM]: y must be a finite number.\n";
                    return 2;
                }
            } else {
                std::cerr << "error[INVALID_ARGUMENT]: unknown or incomplete option '" << option << "'\n";
                return 2;
            }
        }

        const auto result = motorforge::create_entity(
            project, scene_id, id, name, x, y, sprite, expected_revision);
        return report_scene_result(result, json_output);
    }

    if (command == "spritegen") {
        if (argc < 3) {
            std::cerr << "error[ARGUMENT_REQUIRED]: use 'motorforge spritegen <output.svg> [--type industrial-building] [--seed <uint32>] [--size <16..1024>] [--json]'\n";
            return 2;
        }

        const std::string output_path{argv[2]};
        std::string type{"industrial-building"};
        std::uint32_t seed{1};
        int size{64};
        bool json_output = false;

        for (int index = 3; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--json") {
                json_output = true;
            } else if (option == "--type" && index + 1 < argc) {
                type = argv[++index];
            } else if (option == "--seed" && index + 1 < argc) {
                if (!parse_integer<std::uint32_t>(argv[++index], seed)) {
                    std::cerr << "error[INVALID_SEED]: seed must be an unsigned 32-bit integer.\n";
                    return 2;
                }
            } else if (option == "--size" && index + 1 < argc) {
                if (!parse_integer<int>(argv[++index], size)) {
                    std::cerr << "error[INVALID_SPRITE_SIZE]: size must be an integer.\n";
                    return 2;
                }
            } else {
                std::cerr << "error[INVALID_ARGUMENT]: unknown or incomplete option '"
                          << option << "'\n";
                return 2;
            }
        }

        const auto result = motorforge::write_procedural_sprite(output_path, type, seed, size);
        if (json_output) {
            std::cout << motorforge::sprite_generation_result_json(result) << '\n';
        } else if (result.ok) {
            std::cout << "Generated procedural sprite: "
                      << result.output_path.generic_string() << '\n';
        } else {
            for (std::size_t index = 0; index < result.error_codes.size(); ++index) {
                std::cerr << "error[" << result.error_codes[index] << "]: "
                          << result.messages[index] << '\n';
            }
        }

        return result.ok ? 0 : 1;
    }

    std::cerr << "error[UNKNOWN_COMMAND]: unknown command '" << command
              << "'. Run 'motorforge --help'.\n";
    return 2;
}
