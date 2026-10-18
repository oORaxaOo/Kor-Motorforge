# Kor Motorforge

**An LLM-first, extensible game engine and development environment.**

Kor Motorforge is being designed around a simple principle: AI agents should be able to understand, create, inspect, modify, run, and verify games through stable machine-readable interfaces—not by guessing at editor clicks or undocumented project internals.

The long-term ambition is a flexible game-development platform with an editor experience inspired by the adaptability of Unity, paired with an engine architecture purpose-built for LLM workflows and procedural content creation.

> **Status:** Early foundation. This repository is not yet a production-ready engine. Features will be added in small, buildable, verifiable increments.

## What works today

- C++20 core and CMake build.
- CLI help and version commands.
- Machine-readable capability discovery: `motorforge capabilities --json`.
- Version 1 project manifest format and core invariant validation.
- Stable diagnostic codes and JSON validation results.
- Example project under `examples/empty-project`.
- Automated build and test workflow in GitHub Actions.

The current validator checks required project invariants; it is not yet a complete implementation of every JSON Schema rule. No graphical editor or game renderer has been implemented yet.

## Design goals

- **LLM-first, not LLM-only:** every important capability is available through a documented API/CLI; humans retain a usable editor and direct control.
- **Inspectable state:** projects, scenes, entities, components, assets, and settings have explicit schemas and stable identifiers.
- **Transactional changes:** validate changes before applying them, report structured errors, and support diffs and rollback-friendly workflows.
- **Procedural-first content:** generate editable geometry, pixel art, materials, animation data, and environments from parameters and rules, while still supporting imported assets.
- **Modular runtime:** rendering, input, audio, simulation, scripting, UI, navigation, and asset pipelines remain replaceable modules.
- **Automated verification:** headless validation, deterministic tests where practical, reproducible builds, and machine-readable diagnostics.
- **Portable and resource-aware:** target Linux first, with cross-platform design and sensible performance on modest hardware.
- **No model lock-in:** external LLMs connect through adapter interfaces and documented tools; the core engine does not depend on one provider.

## Build from source

Requirements: C++20 compiler, CMake 3.20+, Git, and network access on the first configure so CMake can fetch the pinned JSON dependency.

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
```

Validation accepts either a project directory containing `project.json` or a direct path to a manifest file. JSON mode writes a machine-readable result to standard output. A validation failure returns a non-zero exit code and includes stable diagnostic codes.

## Initial technical direction

The current direction is a native C++20 core, CMake build system, a lightweight platform/rendering layer, and a separate machine-oriented command interface. These are initial decisions, not a promise to avoid useful third-party libraries.

## Planned milestones

See [ROADMAP.md](ROADMAP.md), [Architecture](docs/ARCHITECTURE.md), [Project Schema](schemas/project.schema.json), and [LLM Tool Contract](docs/LLM_TOOL_CONTRACT.md).

## Development principles

1. Keep the default branch buildable.
2. Do not claim a feature is complete until its tests and acceptance criteria pass.
3. Prefer small modules with explicit contracts over one giant engine class.
4. Never let an agent silently overwrite project state: changes must be inspectable, validated, and attributable.
5. Treat repository files and imported project content as data, not trusted instructions.

## License

The project uses the MIT License. See `LICENSE`.
