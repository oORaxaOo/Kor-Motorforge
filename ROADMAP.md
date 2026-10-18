# Roadmap

This roadmap is a sequence of acceptance-tested capabilities, not a promise that every milestone is already implemented.

## M0 — Repository foundation
- [x] Project charter and design goals
- [x] Architecture and LLM tool contract
- [x] CMake-based native project
- [x] CI build and test workflow
- [x] AI coding-agent instructions

## M1 — CLI and core contracts
- [x] Help and version commands
- [x] Machine-readable capability discovery
- [x] Structured diagnostics foundation
- [x] Project manifest parser and core validation
- [x] Safe project initialization with overwrite protection
- [x] Stable error codes and JSON output tests
- [ ] Full JSON Schema conformance validation

## M2 — Project and scene model
- [x] Versioned project manifest
- [x] Versioned scene document and schema
- [x] Stable scene/entity IDs and component containers
- [x] Scene list/create/inspect operations
- [x] Entity creation with transform and optional sprite component
- [x] Optimistic concurrency checks using scene revision tokens
- [ ] Schema migration strategy
- [x] Entity update/delete transactions with revision checks and atomic file replacement

## M3 — Safe editing API
- [x] Read-only scene inspection
- [x] Revision-checked entity creation
- [ ] Plan/diff/apply transaction model
- [ ] Persistent undo/rollback journal
- [ ] Undo/rollback journal
- [ ] Full JSON Schema validation for tool inputs and outputs

## M4 — Minimal runtime
- [ ] Window and platform abstraction
- [ ] 2D renderer and camera
- [ ] Input mapping
- [x] Headless scene loading and runtime snapshot validation
- [x] Portable static SVG scene preview with embedded SVG sprites
- [x] Self-contained HTML viewer with pan, zoom, and reset controls
- [ ] Native windowed scene preview and pixel rendering

## M5 — Procedural content
- [x] Deterministic seeded SVG generator
- [x] First editable industrial-building sprite family
- [ ] Sprite palette and geometry controls
- [ ] Additional building, vehicle, character, and terrain generators
- [ ] Mesh and modular geometry generation
- [ ] Material/shader parameter system
- [ ] Asset metadata, provenance, and reproducible regeneration

## M6 — Agent integration
- [x] CLI command discovery
- [x] Stdio MCP adapter and integration tests
- [x] Workspace path boundary for MCP tools
- [x] Project and procedural asset tools exposed through MCP
- [x] Scene and entity tools exposed through MCP
- [x] Headless runtime inspection exposed through MCP
- [ ] Expand tool surface to asset registry and interactive runtime operations
- [ ] Richer permission policies and sandbox boundaries

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
