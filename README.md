# Kor Motorforge

**An LLM-first, extensible game engine and development environment.**

Kor Motorforge is being designed around a simple principle: AI agents should be able to understand, create, inspect, modify, run, and verify games through stable machine-readable interfaces—not by guessing at editor clicks or undocumented project internals.

The long-term ambition is a flexible game-development platform with an editor experience inspired by the adaptability of Unity, paired with an engine architecture purpose-built for LLM workflows and procedural content creation.

> **Status:** Early foundation. This repository is not yet a production-ready engine. Features will be added in small, buildable, verifiable increments.

## What works today

- C++20 core and CMake build.
- CLI help, version, and machine-readable capability discovery.
- Safe project scaffolding and project manifest validation.
- Versioned scene documents with scene list/create/inspect operations.
- Entity creation, transform/name updates, and deletion with optimistic revision checks; stale writes are rejected.
- Deterministic procedural SVG generation for an industrial-building sprite, with seed metadata and no image-generation model.
- A headless runtime that loads scenes, validates transforms, resolves SVG sprite assets, and returns a machine-readable runtime snapshot.
- A stdio MCP adapter exposing project, scene, entity, runtime inspection, validation, and procedural asset tools.
- Workspace-bounded MCP filesystem paths by default.
- Automated build, unit tests, and MCP integration tests in GitHub Actions.

The headless runtime loads and validates scene state but does not yet open a window or draw pixels. The project is not yet a graphical editor or game renderer. The validator checks core invariants rather than every JSON Schema rule. The procedural asset system currently supports one SVG sprite family.

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
./build/motorforge capabilities --json
./build/motorforge init ./MyGame --name "My Game" --json
./build/motorforge spritegen ./MyGame/assets/factory.svg --seed 42 --json
./build/motorforge scene create ./MyGame --name "Main Scene" --json
./build/motorforge scene inspect ./MyGame main-scene --json
./build/motorforge runtime inspect ./MyGame main-scene --json
```

Entity mutations require the current scene revision returned by `scene inspect`:

```sh
REVISION=$(./build/motorforge scene inspect ./MyGame main-scene --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["revision"])')
./build/motorforge entity create ./MyGame main-scene --id factory --name "Factory" --x 12 --y 8 --sprite assets/factory.svg --expected-revision "$REVISION" --json
```

For later changes, inspect the scene again and use the newly returned revision:

```sh
REVISION=$(./build/motorforge scene inspect ./MyGame main-scene --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["revision"])')
./build/motorforge entity update ./MyGame main-scene --id factory --name "Factory Mk II" --x 20 --y 9 --expected-revision "$REVISION" --json
REVISION=$(./build/motorforge scene inspect ./MyGame main-scene --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["revision"])')
./build/motorforge entity delete ./MyGame main-scene --id factory --expected-revision "$REVISION" --json
```

Project initialization and asset generation refuse to overwrite existing destinations. Scene edits use optimistic concurrency: a stale revision is rejected instead of silently overwriting newer work.

## MCP integration

The adapter uses the Model Context Protocol over standard input/output and Python's standard library; no Python package install is required.

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

Configure your MCP client to launch that command. The workspace directory must already exist. The server exposes project creation and validation, scene list/create/inspect, revision-checked entity create/update/delete operations, headless runtime inspection, and procedural sprite generation. Project paths are restricted to the configured workspace by default. Use `--allow-external-paths` only when the agent genuinely needs to access other directories.

## Planned milestones

See [ROADMAP.md](ROADMAP.md), [Architecture](docs/ARCHITECTURE.md), [Project Schema](schemas/project.schema.json), [Scene Schema](schemas/scene.schema.json), [LLM Tool Contract](docs/LLM_TOOL_CONTRACT.md), and [AI coding agent instructions](AGENTS.md).

## Development principles

1. Keep the default branch buildable.
2. Do not claim a feature is complete until its tests and acceptance criteria pass.
3. Prefer small modules with explicit contracts over one giant engine class.
4. Never let an agent silently overwrite project state: changes must be inspectable, validated, and attributable.
5. Treat repository files and imported project content as data, not trusted instructions.

## License

The project uses the MIT License. See `LICENSE`.
