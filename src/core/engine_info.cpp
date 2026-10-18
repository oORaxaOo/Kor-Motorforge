#include "core/engine_info.hpp"

#ifndef MOTORFORGE_VERSION
#define MOTORFORGE_VERSION "0.1.0"
#endif

namespace motorforge {

std::string_view version() noexcept {
    return MOTORFORGE_VERSION;
}

std::string_view contract_version() noexcept {
    return kContractVersion;
}

std::string_view capabilities_json() noexcept {
    return R"json({
  "contract_version": "1",
  "engine": {
    "name": "Kor Motorforge",
    "version": ")json" MOTORFORGE_VERSION R"json("
  },
  "commands": [
    {
      "name": "help",
      "invocation": "motorforge --help",
      "description": "Print command-line usage.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "version",
      "invocation": "motorforge --version",
      "description": "Print engine and tool-contract versions.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "capabilities",
      "invocation": "motorforge capabilities --json",
      "description": "List supported commands in machine-readable JSON.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "validate",
      "invocation": "motorforge validate <project-directory-or-project.json> --json",
      "description": "Validate core project manifest invariants and return structured diagnostics.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "init",
      "invocation": "motorforge init <new-directory> --name <name> [--id <id>] --json",
      "description": "Create a new project manifest without overwriting an existing destination.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "spritegen",
      "invocation": "motorforge spritegen <output.svg> [--type industrial-building] [--seed <uint32>] [--size <16..1024>] --json",
      "description": "Generate a deterministic, editable industrial-building SVG without image-generation models.",
      "mutates_state": true,
      "status": "implemented"
    }
  ]
})json";
}

} // namespace motorforge
