# Roadmap

This roadmap is a sequence of acceptance-tested capabilities, not a promise that every milestone is already implemented.

## M0 — Repository foundation
- [x] Project charter and design goals
- [x] Architecture and LLM tool contract
- [x] CMake-based native project
- [x] CI build and test workflow
- [x] First GitHub Actions build and test run verified
- [x] AI coding-agent instructions

## M1 — CLI and core contracts
- [x] Help and version commands
- [x] Machine-readable capability discovery
- [x] Structured diagnostics foundation
- [x] Project manifest parser and core validation
- [x] Safe project initialization with overwrite protection
- [x] Stable error codes and JSON output tests
- [ ] Full JSON Schema conformance validation (current validator checks required core invariants)

## M2 — Project model
- [x] Versioned project manifest
- [ ] Stable IDs for scenes, entities, components, and assets
- [x] Schema discovery foundation
- [ ] Schema migration strategy
- [x] Validation without launching the editor

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
- [x] Deterministic seeded SVG generator
- [x] First editable industrial-building sprite family
- [ ] Sprite palette and geometry controls
- [ ] Additional building, vehicle, character, and terrain generators
- [ ] Mesh and modular geometry generation
- [ ] Material/shader parameter system
- [ ] Asset metadata, provenance, and reproducible regeneration

## M6 — Agent integration
- [x] CLI command schema discovery foundation
- [x] Initial stdio MCP adapter and integration test
- [x] Workspace path boundary for MCP tools
- [x] Procedural sprite generation exposed through MCP
- [ ] Expand tool surface to scene, entity, asset, and runtime operations
- [ ] Agent-safe permission policies and richer sandbox boundaries

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
