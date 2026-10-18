# Contributing

Kor Motorforge is being built in small, verifiable increments.

## Before changing code
1. Read [the architecture](docs/ARCHITECTURE.md) and [LLM tool contract](docs/LLM_TOOL_CONTRACT.md).
2. Keep changes focused and update the roadmap when a milestone changes.
3. Do not advertise unimplemented commands as supported capabilities.

## Build and test

Requirements: a C++20 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/motorforge --help
./build/motorforge capabilities --json
```

## Change requirements
- Add tests for new behavior and regression tests for bug fixes.
- Keep machine-readable output valid JSON and version the tool contract when it changes incompatibly.
- Use stable error codes for expected failures.
- Prefer explicit schemas and stable IDs over implicit naming conventions.
- Avoid introducing large dependencies before documenting their purpose and maintenance cost.
- Never commit secrets, credentials, private keys, or generated build directories.

## Pull requests
Describe the behavior changed, tests run, remaining limitations, and any schema/API compatibility impact. A successful compile alone is not proof of correct runtime behavior.
