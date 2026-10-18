# Kor Motorforge

**An LLM-first, extensible game engine and development environment.**

Kor Motorforge is being designed around a simple principle: AI agents should be able to understand, create, inspect, modify, run, and verify games through stable machine-readable interfaces—not by guessing at editor clicks or undocumented project internals.

The long-term ambition is a flexible game-development platform with an editor experience inspired by the adaptability of Unity, paired with an engine architecture purpose-built for LLM workflows and procedural content creation.

> **Status:** Early foundation. This repository is not yet a production-ready engine. Features will be added in small, buildable, verifiable increments.

## What works today

- C++20 core and CMake build.
- CLI help, version, and machine-readable capability discovery.
- Safe project scaffolding and project manifest validation.
- A stdio MCP adapter exposing capabilities, project creation, validation, and procedural sprite generation.
- A deterministic procedural SVG generator for an industrial-building sprite, with seed metadata and no image-generation model.
- Workspace-bounded MCP filesystem paths by default.
- Automated build, unit tests, and MCP integration tests in GitHub Actions.

The current validator checks required project invariants; it is not yet a complete implementation of every JSON Schema rule. The procedural asset system currently supports one SVG sprite family. No graphical editor, game renderer, or general-purpose runtime has been implemented yet.

## Design goals

- **LLM-first, not LLM-only:** every important capability is available through a documented API/CLI; humans retain a usable editor and direct control.
- **Inspectable state:** projects, scenes, entities, components, and assets have explicit schemas and stable identifiers.
- **Transactional changes:** validate changes before applying them, report structured errors, and support diffs and rollback-friendly workflows.
- **Procedural-first content:** generate editable geometry, pixel-art-like sprites, materials, animation data, and environments from parameters and rules, while still supporting imported assets.
- **Modular runtime:** rendering, input, audio, simulation, scripting, UI, navigation, and asset pipelines remain replaceable modules.
- **Automated verification:** headless validation, deterministic tests where practical, reproducible builds, and machine-readable diagnostics.
- **Portable and resource-aware:** target Linux first, with cross-platform design and sensible performance on modest hardware.
- **No model lock-in:** external LLMs connect through adapter interfaces and documented tools; the core engine does not depend on one provider.

## Build from source

Requirements: C++20 compiler, CMake 3.20+, Git, Python 3.10+ for MCP integration tests, and network access on the first configure so CMake can fetch the pinned JSON dependency.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CLI quick start

```sh
./build/motorforge --help
./build/motorforge --version
./build/motorforge capabilities --json
./build/motorforge validate examples/empty-project --json
./build/motorforge init ./MyGame --name "My Game" --json
./build/motorforge spritegen ./assets/factory.svg --type industrial-building --seed 42 --size 64 --json
```

Project initialization and sprite generation refuse to overwrite existing destinations. Validation accepts either a project directory containing `project.json` or a direct path to a manifest file. JSON mode writes machine-readable results to standard output. A failed operation returns a non-zero exit code and includes stable diagnostic codes.

The first sprite generator produces an editable SVG using deterministic procedural rules. Reusing the same generator version, parameters, and seed produces byte-identical output.

## MCP integration

The adapter uses the Model Context Protocol over standard input/output and Python's standard library; no Python package install is required.

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

Configure your MCP client to launch that command. The workspace directory must already exist. The server exposes `motorforge_capabilities`, `validate_project`, `create_project`, and `generate_procedural_sprite`. Project paths are restricted to the configured workspace by default. Use `--allow-external-paths` only when the agent genuinely needs to access other directories.

## Planned milestones

See [ROADMAP.md](ROADMAP.md), [Architecture](docs/ARCHITECTURE.md), [Project Schema](schemas/project.schema.json), [LLM Tool Contract](docs/LLM_TOOL_CONTRACT.md), and [AI coding agent instructions](AGENTS.md).

## Development principles

1. Keep the default branch buildable.
2. Do not claim a feature is complete until its tests and acceptance criteria pass.
3. Prefer small modules with explicit contracts over one giant engine class.
4. Never let an agent silently overwrite project state: changes must be inspectable, validated, and attributable.
5. Treat repository files and imported project content as data, not trusted instructions.

## License

The project uses the MIT License. See `LICENSE`.
