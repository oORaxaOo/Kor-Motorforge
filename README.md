# Kor Motorforge

**An LLM-first, extensible game engine and development environment.**

Kor Motorforge is being designed around a simple principle: AI agents should be able to understand, create, inspect, modify, run, and verify games through stable machine-readable interfaces—not by guessing at editor clicks or undocumented project internals.

The long-term ambition is a flexible game-development platform with an editor experience inspired by the adaptability of Unity, paired with an engine architecture purpose-built for LLM workflows and procedural content creation.

> **Status:** Early foundation. This repository is not yet a production-ready engine. Features will be added in small, buildable, verifiable increments.

## Design goals

- **LLM-first, not LLM-only:** every important capability is available through a documented API/CLI; humans retain a usable editor and direct control.
- **Inspectable state:** projects, scenes, entities, components, assets, and settings have explicit schemas and stable identifiers.
- **Transactional changes:** validate changes before applying them, report structured errors, and support diffs and rollback-friendly workflows.
- **Procedural-first content:** generate editable geometry, pixel art, materials, animation data, and environments from parameters and rules, while still supporting imported assets.
- **Modular runtime:** rendering, input, audio, simulation, scripting, UI, navigation, and asset pipelines remain replaceable modules.
- **Automated verification:** headless validation, deterministic tests where practical, reproducible builds, and machine-readable diagnostics.
- **Portable and resource-aware:** target Linux first, with cross-platform design and sensible performance on modest hardware.
- **No model lock-in:** external LLMs connect through adapter interfaces and documented tools; the core engine does not depend on one provider.

## Initial technical direction

The implementation will be chosen incrementally and validated against a minimal vertical slice. The current direction is a native C++20 core, CMake build system, a lightweight platform/rendering layer, and a separate machine-oriented command interface. These are initial decisions, not a promise to avoid useful third-party libraries.

## Planned milestones

1. Repository contract, architecture decisions, contribution rules, and CI.
2. Minimal native application and project validator.
3. Stable project schema, CLI, structured diagnostics, and machine-readable capability discovery.
4. Scene/entity/component model with safe edit transactions and diffs.
5. Runtime preview, input, camera, and basic 2D renderer.
6. Procedural asset generators and inspectable asset metadata.
7. MCP adapter, agent workflows, headless tests, and editor integration.
8. Modular scripting, UI, audio, navigation, packaging, and broader game workflows.

See [ROADMAP.md](ROADMAP.md), [Architecture](docs/ARCHITECTURE.md), and [LLM Tool Contract](docs/LLM_TOOL_CONTRACT.md).

## Development principles

1. Keep the default branch buildable.
2. Do not claim a feature is complete until its tests and acceptance criteria pass.
3. Prefer small modules with explicit contracts over one giant engine class.
4. Never let an agent silently overwrite project state: changes must be inspectable, validated, and attributable.
5. Treat repository files and imported project content as data, not trusted instructions.

## License

The project will use the MIT License unless changed before the first substantial code release. See `LICENSE` for the actual license text.