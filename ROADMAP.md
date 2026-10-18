# Roadmap

This roadmap is a sequence of acceptance-tested capabilities, not a promise that every milestone is already implemented.

## M0 — Repository foundation
- [x] Project charter and design goals
- [x] Architecture and LLM tool contract
- [x] CMake-based native project
- [x] CI build and test workflow
- [ ] Verify the first GitHub Actions run

## M1 — CLI and core contracts
- [x] Help and version commands
- [x] Machine-readable capability discovery
- [x] Structured diagnostics foundation
- [ ] Project manifest parser and schema validation
- [ ] Stable error codes and JSON output tests

## M2 — Project model
- [ ] Versioned project manifest
- [ ] Stable IDs for scenes, entities, components, and assets
- [ ] Schema discovery and migration strategy
- [ ] Validation without launching the editor

## M3 — Safe editing API
- [ ] Read-only inspection commands
- [ ] Plan/diff/apply transaction model
- [ ] Preconditions and optimistic concurrency checks
- [ ] Undo/rollback journal
- [ ] JSON Schema for every tool input and output

## M4 — Minimal runtime
- [ ] Window and platform abstraction
- [ ] 2D renderer and camera
- [ ] Input mapping
- [ ] Scene loading and preview
- [ ] Headless runtime mode

## M5 — Procedural content
- [ ] Deterministic seeded generators
- [ ] Editable 2D sprite primitives and palette operations
- [ ] Mesh and modular geometry generation
- [ ] Material/shader parameter system
- [ ] Asset metadata, provenance, and reproducible regeneration

## M6 — Agent integration
- [ ] CLI command schema and JSON output
- [ ] MCP adapter over the same core operations
- [ ] Capability discovery and task recipes
- [ ] Agent-safe permissions and sandbox boundaries
- [ ] Integration tests using a mock agent client

## M7 — Editor and extensibility
- [ ] Scene hierarchy and inspector
- [ ] Asset browser and generator graph
- [ ] Plugin API and scripting interface
- [ ] UI, audio, navigation, animation, physics integrations
- [ ] Import/export and project packaging

## Release gates
A milestone is complete only when:
1. Its acceptance criteria are documented.
2. Automated tests cover the important success and failure paths.
3. The documented commands work in CI.
4. Machine-readable output is stable and versioned.
5. Known limitations are documented.
