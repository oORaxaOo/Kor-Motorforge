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
    },
    {
      "name": "scene.list",
      "invocation": "motorforge scene list <project> --json",
      "description": "List valid scenes and their current revision tokens.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "scene.create",
      "invocation": "motorforge scene create <project> --name <name> [--id <id>] --json",
      "description": "Create a versioned scene document without overwriting existing scenes.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "scene.inspect",
      "invocation": "motorforge scene inspect <project> <scene-id> --json",
      "description": "Read scene data and its revision token before making edits.",
      "mutates_state": false,
      "status": "implemented"
    },
    {
      "name": "entity.create",
      "invocation": "motorforge entity create <project> <scene-id> --id <id> --name <name> --expected-revision <revision> --json",
      "description": "Create an entity using optimistic concurrency and a relative asset path.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "entity.update",
      "invocation": "motorforge entity update <project> <scene-id> --id <id> --name <name> --x <number> --y <number> --expected-revision <revision> --json",
      "description": "Update entity name and position with revision checking.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "entity.delete",
      "invocation": "motorforge entity delete <project> <scene-id> --id <id> --expected-revision <revision> --json",
      "description": "Delete an entity with revision checking.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "preview.interactive",
      "invocation": "motorforge preview interactive <project> <scene-id> --output <file.html> [--width <320..4096>] [--height <240..4096>] --json",
      "description": "Export a self-contained HTML scene viewer with pointer-based pan and zoom controls.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "preview.render",
      "invocation": "motorforge preview render <project> <scene-id> --output <file.svg> [--width <320..4096>] [--height <240..4096>] --json",
      "description": "Export a portable static SVG scene preview with embedded sprite assets.",
      "mutates_state": true,
      "status": "implemented"
    },
    {
      "name": "runtime.inspect",
      "invocation": "motorforge runtime inspect <project> <scene-id> --json",
      "description": "Load and validate a scene in headless mode and resolve SVG sprite assets.",
      "mutates_state": false,
      "status": "implemented"
    }
  ]
})json";
}

} // namespace motorforge
