# LLM Tool Contract

This document defines the contract all agent-facing interfaces must follow. The CLI is the first client; MCP and editor integrations should reuse the same operations rather than implement parallel business logic.

## Requirements

### Machine-readable mode
- Commands intended for agents support JSON output.
- Standard output contains the requested result only in JSON mode.
- Human-oriented progress and diagnostics go to standard error.
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
Agents must be able to ask the tool surface:
- which commands exist;
- which contract version they implement;
- what arguments each command accepts;
- whether the command reads or mutates state;
- what permissions it requires;
- how success and failure are represented.

## Implemented CLI contract

- `--help`: human-readable usage.
- `--version`: engine and contract versions.
- `capabilities --json`: machine-readable list of implemented commands.
- `validate <project-directory-or-project.json> [--json]`: validate a project manifest.
- `init <new-directory> --name <name> [--id <id>] [--json]`: create a project without overwriting an existing destination.

## MCP tools

The stdio adapter exposes the same operations to MCP-compatible clients:

- `motorforge_capabilities`: read command and capability metadata.
- `validate_project`: validate a project manifest.
- `create_project`: create a new project manifest without overwriting an existing destination.

The adapter invokes only explicit CLI commands with argument arrays (never a shell), applies a timeout, and restricts paths to a configured workspace by default. The workspace boundary is a useful safety guard, not a complete security sandbox; only connect trusted clients and keep the engine process permissions appropriately limited.

Start it with:

```sh
python3 tools/mcp_server.py --motorforge ./build/motorforge --workspace ./my-projects
```

## Project manifest validation

The validator currently checks core invariants: supported schema version, project ID and name, engine minimum version format, and settings object type. The versioned schema lives at `schemas/project.schema.json`. Full JSON Schema conformance validation and migration tooling are future work.

The initializer creates a version 1 manifest with deterministic defaults. If no ID is provided, it derives a lowercase ASCII slug from the display name. It refuses to overwrite an existing destination. Creation results and validation results include `contract_version` and machine-readable status fields.

## Validation response

A successful validation response includes `contract_version`, `ok`, `manifest_path`, `project`, and a `diagnostics` array. Each diagnostic has a stable `code`, a readable `message`, and a `location`.

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
