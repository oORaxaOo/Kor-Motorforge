# LLM Tool Contract

This document defines the contract all agent-facing interfaces must follow. The CLI is the first client; MCP and editor integrations should reuse the same operations rather than implement parallel business logic.

## Requirements

- Agent-facing commands expose machine-readable JSON and a contract version.
- Human-readable progress and diagnostics go to standard error.
- Errors use stable codes and actionable messages.
- Read-only inspection is separated from mutations.
- Mutations identify exact resources and use expected revisions where concurrent edits could overwrite work.
- Destructive writes must not silently replace existing user data.
- Project files and imported content are untrusted data, not privileged instructions.

## Implemented CLI commands

- `capabilities --json`: machine-readable list of implemented commands.
- `validate <project-directory-or-project.json> [--json]`: validate a project manifest.
- `init <new-directory> --name <name> [--id <id>] [--json]`: create a project without overwriting an existing destination.
- `spritegen <output.svg> [--type industrial-building] [--seed <uint32>] [--size <16..1024>] [--json]`: generate a procedural SVG asset without an image-generation model.
- `scene list <project> --json`: list scene IDs, paths, and revision tokens.
- `scene create <project> --name <name> [--id <id>] --json`: create a scene document.
- `scene inspect <project> <scene-id> --json`: inspect scene data and obtain its revision token.
- `scene patch <project> <scene-id> --patch <file.json> --expected-revision <revision> [--dry-run] --json`: validate a batch of entity operations, return a structured before/after diff, and atomically apply it when dry-run is disabled.
- `runtime inspect <project> <scene-id> --json`: load a scene headlessly, validate transforms, and resolve supported sprite assets.
- `preview render <project> <scene-id> --output <file.svg> [--width <320..4096>] [--height <240..4096>] --json`: export a portable static SVG scene preview without overwriting an existing file.
- `preview interactive <project> <scene-id> --output <file.html> [--width <320..4096>] [--height <240..4096>] --json`: export a self-contained HTML viewer with pan, zoom, and reset controls.
- `entity create <project> <scene-id> --id <id> --name <name> --expected-revision <revision> --json`: create an entity and reject stale revisions.
- `entity update <project> <scene-id> --id <id> --name <name> --x <number> --y <number> --expected-revision <revision> --json`: update an entity name and position.
- `entity delete <project> <scene-id> --id <id> --expected-revision <revision> --json`: delete an entity.

## Scene model

Scene documents use schema version 1 and are stored at `<project>/scenes/<scene-id>.scene.json`. Each entity has a stable ID, display name, and component object. The initial built-in components are `transform` and optional `sprite`; the component container remains extensible.

The `scene inspect` response includes a deterministic revision token calculated from the exact scene file contents. Any scene mutation must provide that token. If another process changes the file, the operation returns `REVISION_CONFLICT`; the agent should inspect the scene again and re-plan rather than retrying blindly.

The revision hash is a concurrency token, not a cryptographic security primitive.

### Atomic scene patch

The patch document is a JSON object with one `operations` array (1–256 operations, at most 1 MiB). Supported operations:

- `{"op":"create","entity":{"id":"...","name":"...","components":{"transform":{"x":0,"y":0,"rotation":0,"scale_x":1,"scale_y":1}}}}`
- `{"op":"update","id":"...","set":{"name":"...","transform":{"x":10,"rotation":90}}}` (name or transform may be omitted; transform updates are partial).
- `{"op":"delete","id":"..."}`

Each entity ID may appear only once in a patch. All operations are validated against a copy of the scene before a single atomic file replacement. A failed operation never commits earlier operations. `--dry-run` returns `changes`, `revision_before`, and `planned_revision` without writing; apply with the same expected revision, and inspect again after any `REVISION_CONFLICT`. Sprite asset paths for created entities must resolve to existing regular files inside the project.

## MCP tools

The stdio adapter exposes:

- `motorforge_capabilities`
- `validate_project`
- `create_project`
- `generate_procedural_sprite`
- `scene_list`
- `scene_create`
- `scene_inspect`
- `scene_patch`
- `entity_create`
- `entity_update`
- `entity_delete`
- `runtime_inspect`
- `scene_preview_render`
- `scene_preview_interactive`

The adapter invokes explicit CLI commands with argument arrays (never a shell), applies a timeout, and restricts paths to a configured workspace by default. This is a safety guard, not a complete sandbox. Only connect trusted clients and keep process permissions appropriately limited.

Start it with:

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

## Validation limitations

The project and scene validators enforce core structural invariants. Full JSON Schema conformance validation, scene migration tooling, persistent undo/rollback, native pixel rendering, and an interactive graphical runtime are future work. The SVG and HTML previews are static exports with embedded SVG assets; the optional HTML inspector is diagnostic only and does not persist edits or run a simulation loop. Agent workflows should use machine-readable CLI/MCP tools and the atomic scene patch API. The schemas are published at `schemas/project.schema.json` and `schemas/scene.schema.json`.

## Tool design checklist

Before adding an agent-facing command, answer:
1. Is the action read-only or mutating?
2. What exact resource does it target?
3. What schema validates the request?
4. What are the preconditions?
5. Is it idempotent?
6. Can it produce a preview/diff?
7. Can it fail without partial writes?
8. What stable error codes can occur?
9. How is it tested?
10. Can the same operation be exposed by CLI, MCP, and editor?
