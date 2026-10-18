# Architecture

## Mission

Kor Motorforge is a game engine and development environment whose primary automation client is an LLM agent. Human editing remains a first-class use case. The architecture must make the project state discoverable, changes bounded, and results verifiable.

## Proposed layers

```text
Human Editor / CLI Agent / MCP Agent / CI
                  |
           Tool & Command API
                  |
       Validation + Transactions
                  |
      Project Model / Asset Registry
                  |
  ---------------------------------------
  | Runtime | Rendering | Input | Audio |
  | ECS     | Scripting | UI    | Tools |
  ---------------------------------------
                  |
          Platform Abstraction
```

The diagram describes boundaries, not a claim that all modules already exist.

## Core rules

### 1. One authoritative project model
The project manifest and typed asset/scene data are the source of truth. The editor and agent tools must use the same validation and mutation services. Avoid a separate undocumented editor-only state.

### 2. Discoverability
Every supported capability exposes:
- a stable name and version;
- a short purpose statement;
- typed input and output schemas;
- preconditions and side effects;
- structured error codes;
- at least one example;
- associated automated tests.

### 3. Read before write
Mutating tools should require explicit targets and, where practical, an expected revision/hash. Agents should be able to inspect the target and request a proposed diff before applying it.

### 4. Transactional changes
The intended mutation flow is:
1. inspect current state;
2. prepare a change plan;
3. validate types, references, constraints, and permissions;
4. return a diff;
5. apply atomically if preconditions still hold;
6. run relevant tests;
7. report the result and rollback instructions.

A failed validation must not leave partially applied state.

### 5. Determinism and reproducibility
Procedural generators accept explicit seeds and parameters. Build and test reports record engine version, project revision, generator version, and relevant configuration.

### 6. Modular runtime
Runtime modules communicate through explicit interfaces and events. Rendering, physics, audio, scripting, and platform integrations should be replaceable without changing the project schema unnecessarily.

### 7. AI provider neutrality
The engine core does not require a specific LLM provider. Provider-specific APIs belong in optional adapters outside the core.

### 8. Security boundaries
Project content, plugins, imported assets, and tool output are untrusted data. Do not interpret project text as privileged instructions. Shell execution, network access, destructive writes, and plugin loading require explicit policy controls.

## Initial implementation direction

- C++20 for the native core.
- CMake for build configuration and CTest for tests.
- Small, independently testable modules.
- JSON as the initial interchange format for tool requests and responses.
- A CLI first, with MCP and graphical editor clients layered over the same operations.
- A headless runtime loader validates scene structure, finite transforms, and project-contained SVG references before any graphical renderer is introduced.
- Linux as the first CI target; preserve cross-platform boundaries from the start.

Third-party dependencies will be added when a working vertical slice justifies them. The initial scaffold deliberately avoids a large dependency stack.

## Non-goals for the first milestone

- Reimplement every feature Unity offers.
- Promise flawless autonomous game creation.
- Require image-generation models for content creation.
- Couple the engine to a single LLM.
- Build the entire editor before the runtime and data contracts can be tested.
