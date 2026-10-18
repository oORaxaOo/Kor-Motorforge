# Kor Motorforge

**An LLM-first, extensible game engine and development environment.**

Kor Motorforge is being designed around a simple principle: AI agents should be able to understand, create, inspect, modify, run, and verify games through stable machine-readable interfaces—not by guessing at editor clicks or undocumented project internals.

The long-term ambition is a flexible game-development platform with an editor experience inspired by the adaptability of Unity, paired with an engine architecture purpose-built for LLM workflows and procedural content creation.

> **Status:** Early foundation. This repository is not yet a production-ready engine. Features will be added in small, buildable, verifiable increments.

## What works today

- C++20 core and CMake build.
- CLI help and version commands.
- Machine-readable capability discovery: `motorforge capabilities --json`.
- Safe project scaffolding: `motorforge init <path> --name "My Game" --json`.
- Version 1 project manifest format and core invariant validation.
- A stdio MCP adapter exposing capabilities, project creation, and validation tools.
- Workspace-bounded MCP filesystem paths by default.
- Automated build, unit tests, and MCP integration tests in GitHub Actions.

The current validator checks required project invariants; it is not yet a complete implementation of every JSON Schema rule. No graphical editor or game renderer has been implemented yet.

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
```

Project initialization refuses to overwrite an existing destination. Validation accepts either a project directory containing `project.json` or a direct path to a manifest file. JSON mode writes machine-readable results to standard output. A failed operation returns a non-zero exit code and includes stable diagnostic codes.

## MCP integration

The adapter uses the Model Context Protocol over standard input/output and Python's standard library; no Python package install is required.

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

Configure your MCP client to launch that command. The workspace directory must already exist. The server exposes `motorforge_capabilities`, `validate_project`, and `create_project`. Project paths are restricted to the configured workspace by default. Use `--allow-external-paths` only when the agent genuinely needs to access other directories.

## Initial technical direction

The current direction is a native C++20 core, CMake build system, a lightweight platform/rendering layer, and a separate machine-oriented command interface. These are initial decisions, not a promise to avoid useful third-party libraries.

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
