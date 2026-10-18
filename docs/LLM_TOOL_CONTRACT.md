# LLM Tool Contract

This document defines the contract all agent-facing interfaces must follow. The CLI is the first client; MCP and editor integrations should reuse the same operations rather than implement parallel business logic.

## Requirements

### Machine-readable mode
- Commands intended for agents support JSON output.
- Standard output contains the requested result only in JSON mode.
- Human-readable progress and diagnostics go to standard error.
- Output includes a contract version.
- Errors include stable codes, a human-readable message, and optional structured details.

### Safe mutation
- Inspection is read-only.
- Mutations name exact resource IDs and fields.
- A mutation can be validated without applying it.
- Destructive actions require an explicit flag or policy grant.
- When possible, mutations include an expected revision to prevent lost updates.
- Multi-step changes use transactions or a recoverable journal.

### Capability discovery
Agents must be able to ask the tool surface which commands exist, what inputs they accept, whether they mutate state, and how success and failure are represented.

## Implemented CLI commands

- `--help`: human-readable usage.
- `--version`: engine and contract versions.
- `capabilities --json`: machine-readable list of implemented commands.
- `validate <project-directory-or-project.json> [--json]`: validate a project manifest.
- `init <new-directory> --name <name> [--id <id>] [--json]`: create a project without overwriting an existing destination.
- `spritegen <output.svg> [--type industrial-building] [--seed <uint32>] [--size <16..1024>] [--json]`: generate a procedural SVG asset without an image-generation model.

## MCP tools

The stdio adapter exposes these tools to MCP-compatible clients:

- `motorforge_capabilities`: read command and capability metadata.
- `validate_project`: validate a project manifest.
- `create_project`: create a new project manifest without overwriting an existing destination.
- `generate_procedural_sprite`: generate an editable SVG asset from procedural rules.

The adapter invokes only explicit CLI commands with argument arrays (never a shell), applies a timeout, and restricts paths to a configured workspace by default. The workspace boundary is a useful safety guard, not a complete security sandbox; only connect trusted clients and keep the engine process permissions appropriately limited.

Start it with:

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

## Project manifest validation

The validator currently checks core invariants: supported schema version, project ID and name, engine minimum version format, and settings object type. The versioned schema lives at `schemas/project.schema.json`. Full JSON Schema conformance validation and migration tooling are future work.

The initializer creates a version 1 manifest with deterministic defaults. If no ID is provided, it derives a lowercase ASCII slug from the display name. It refuses to overwrite an existing destination.

## Procedural asset contract

The initial `industrial-building` generator creates SVG vector shapes from fixed rules plus a deterministic pseudorandom variation stream. The seed is embedded in the SVG metadata comment. Identical generator version, type, seed, and size produce byte-identical output. Only `.svg` output and sizes from 16 through 1024 are supported in this first implementation.

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
