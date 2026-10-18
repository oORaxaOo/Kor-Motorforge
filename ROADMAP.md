# Roadmap

This roadmap is a sequence of acceptance-tested capabilities, not a promise that every milestone is already implemented. Motorforge is AI-agent infrastructure first: stable machine-readable contracts, deterministic operations, safe project mutation, asset generation and verification take priority over a human-facing editor.

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
- [ ] Full JSON Schema conformance validation

## M2 — Project and scene model
- [x] Versioned project manifest and scene document
- [x] Stable scene/entity IDs and component containers
- [x] Scene list/create/inspect operations
- [x] Entity CRUD with optimistic revision checks
- [x] Atomic scene patch planning and application
- [ ] Schema migration strategy

## M3 — Safe agent mutation
- [x] Read-only scene inspection
- [x] Revision-checked entity mutations
- [x] Plan/diff/apply transaction model for atomic entity create/update/delete batches
- [ ] Persistent undo/rollback journal
- [ ] Full JSON Schema validation for tool inputs and outputs

## M4 — Minimal runtime
- [x] Headless scene loading and runtime snapshot validation
- [x] Portable static SVG scene preview
- [x] Optional self-contained HTML diagnostic inspector
- [ ] Deterministic runtime tick with machine-readable results
- [ ] Native 2D renderer and camera
- [ ] Input mapping

## M5 — AI asset pipeline (priority)
- [x] Deterministic seeded SVG generator for initial industrial-building sprite family
- [x] Versioned animation-asset manifest contract (frame atlas, clips, directions, anchors, frame events, provenance)
- [ ] Asset manifest validator with path containment and atlas-bound checks
- [ ] Image import and metadata probing
- [ ] Deterministic atlas packing and extraction
- [ ] Frame slicing, transparent-background processing and asset validation
- [ ] Animation clip inspection and deterministic preview/export
- [ ] More procedural generators: UI components, vehicles, characters, terrain and effects
- [ ] Optional image-model provider adapters; generation provider remains separate from deterministic processing

## M6 — Agent integration
- [x] CLI command discovery and stdio MCP adapter
- [x] Workspace path boundary for MCP tools
- [x] Project, procedural asset, scene, entity and runtime inspection tools
- [x] AI-facing atomic scene patch with dry-run and revision checks
- [ ] Asset registry, animation tools and generated-asset import over MCP
- [ ] Richer permission policies and sandbox boundaries

## M7 — Runtime and extensibility
- [ ] Animation playback runtime and events
- [ ] UI layout/component schema and deterministic UI asset generation
- [ ] Plugin API and scripting interface
- [ ] Audio, navigation and physics integrations
- [ ] Import/export and project packaging

## Release gates
A milestone is complete only when:
1. Acceptance criteria are documented.
2. Automated tests cover success, malformed input, boundary cases and failure paths.
3. Documented commands work in CI.
4. Machine-readable output is stable and versioned.
5. Known limitations are documented.
