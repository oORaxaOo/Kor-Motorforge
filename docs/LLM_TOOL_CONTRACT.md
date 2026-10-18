# LLM Tool Contract

This document defines the contract all agent-facing interfaces must follow. The CLI is the first client; MCP and editor integrations should reuse the same operations rather than implement parallel business logic.

## Requirements

### Machine-readable mode
- Commands intended for agents support JSON output.
- Standard output contains the requested result only in JSON mode.
- Human-oriented progress and diagnostics go to standard error.
- Output includes a contract version.
- Errors include a stable code, human-readable message, and optional structured details.

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

## Initial CLI contract

The initial native executable supports:
- `--help`: human-readable usage;
- `--version`: version and contract information;
- `capabilities --json`: machine-readable list of currently implemented commands.

Example:

```sh
motorforge --help
motorforge --version
motorforge capabilities --json
```

The first implementation is intentionally small. Commands must not be documented as available until they actually exist and have tests.

## Error shape (target)

```json
{
  "contract_version": "1",
  "ok": false,
  "error": {
    "code": "PROJECT_NOT_FOUND",
    "message": "The project manifest was not found.",
    "details": {}
  }
}
```

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
