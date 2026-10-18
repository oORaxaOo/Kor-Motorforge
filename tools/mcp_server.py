#!/usr/bin/env python3
"""Minimal stdio MCP adapter for the Kor Motorforge CLI.

This adapter intentionally uses Python's standard library and invokes only
explicit Motorforge commands (never a shell). It exposes a workspace boundary
for filesystem paths supplied by an LLM client.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

PROTOCOL_VERSION = "2025-11-25"
SUPPORTED_PROTOCOL_VERSIONS = {PROTOCOL_VERSION, "2025-06-18", "2025-03-26"}
SERVER_VERSION = "0.1.0"

TOOLS: list[dict[str, Any]] = [
    {
        "name": "motorforge_capabilities",
        "description": "Read the engine's machine-readable command and capability registry.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "validate_project",
        "description": "Validate a Motorforge project manifest inside the configured workspace.",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string", "minLength": 1}},
            "required": ["path"],
            "additionalProperties": False,
        },
    },
    {
        "name": "create_project",
        "description": "Create a new Motorforge project inside the configured workspace. Existing destinations are never overwritten.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "minLength": 1},
                "name": {"type": "string", "minLength": 1},
                "id": {"type": "string"},
            },
            "required": ["path", "name"],
            "additionalProperties": False,
        },
    },
]


class ToolFailure(Exception):
    """A safe, user-facing tool failure with a stable code."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


def json_text(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, indent=2)


def resolve_workspace_path(raw_path: Any, workspace: Path, allow_external: bool) -> Path:
    if not isinstance(raw_path, str) or not raw_path.strip():
        raise ToolFailure("INVALID_PATH", "path must be a non-empty string.")

    candidate = Path(raw_path).expanduser()
    if not candidate.is_absolute():
        candidate = workspace / candidate
    candidate = candidate.resolve()

    if not allow_external:
        try:
            candidate.relative_to(workspace)
        except ValueError as error:
            raise ToolFailure(
                "PATH_OUTSIDE_WORKSPACE",
                "The requested path is outside the configured workspace.",
            ) from error

    return candidate


def invoke_motorforge(executable: Path, arguments: list[str]) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            [str(executable), *arguments],
            check=False,
            capture_output=True,
            text=True,
            timeout=60,
            shell=False,
        )
    except subprocess.TimeoutExpired as error:
        raise ToolFailure("ENGINE_COMMAND_TIMEOUT", "Motorforge command exceeded the 60-second timeout.") from error
    except OSError as error:
        raise ToolFailure("ENGINE_EXECUTABLE_UNAVAILABLE", f"Could not run Motorforge: {error}") from error

    output = completed.stdout.strip()
    if not output:
        output = completed.stderr.strip() or json_text({
            "ok": False,
            "error": {"code": "EMPTY_ENGINE_RESPONSE", "message": "Motorforge returned no output."},
        })
    return completed.returncode, output


def call_tool(name: str, arguments: Any, executable: Path,
              workspace: Path, allow_external: bool) -> dict[str, Any]:
    if not isinstance(arguments, dict):
        raise ToolFailure("INVALID_ARGUMENTS", "Tool arguments must be a JSON object.")

    if name == "motorforge_capabilities":
        if arguments:
            raise ToolFailure("UNEXPECTED_ARGUMENTS", "motorforge_capabilities accepts no arguments.")
        return_code, output = invoke_motorforge(executable, ["capabilities", "--json"])
    elif name == "validate_project":
        path = resolve_workspace_path(arguments.get("path"), workspace, allow_external)
        if set(arguments) != {"path"}:
            raise ToolFailure("INVALID_ARGUMENTS", "validate_project accepts only the path argument.")
        return_code, output = invoke_motorforge(executable, ["validate", str(path), "--json"])
    elif name == "create_project":
        if set(arguments) - {"path", "name", "id"} or "path" not in arguments or "name" not in arguments:
            raise ToolFailure("INVALID_ARGUMENTS", "create_project requires path and name; only id is optional.")
        path = resolve_workspace_path(arguments.get("path"), workspace, allow_external)
        project_name = arguments.get("name")
        project_id = arguments.get("id")
        if not isinstance(project_name, str) or not project_name.strip():
            raise ToolFailure("INVALID_PROJECT_NAME", "name must be a non-empty string.")
        if project_id is not None and not isinstance(project_id, str):
            raise ToolFailure("INVALID_PROJECT_ID", "id must be a string when provided.")

        command = ["init", str(path), "--name", project_name, "--json"]
        if project_id:
            command.extend(["--id", project_id])
        return_code, output = invoke_motorforge(executable, command)
    else:
        raise ToolFailure("UNKNOWN_TOOL", f"Unknown Motorforge tool: {name}")

    return {
        "content": [{"type": "text", "text": output}],
        "isError": return_code != 0,
    }


def rpc_error(request_id: Any, code: int, message: str) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def handle_message(message: Any, executable: Path, workspace: Path,
                   allow_external: bool) -> dict[str, Any] | None:
    if not isinstance(message, dict) or message.get("jsonrpc") != "2.0":
        return rpc_error(message.get("id") if isinstance(message, dict) else None,
                         -32600, "Invalid JSON-RPC request.")

    request_id = message.get("id")
    method = message.get("method")
    params = message.get("params") or {}

    if not isinstance(method, str) or not isinstance(params, dict):
        return rpc_error(request_id, -32600, "Invalid JSON-RPC method or params.")

    # Notifications have no ID and must not receive a response.
    if "id" not in message:
        return None

    if method == "initialize":
        requested_version = params.get("protocolVersion")
        negotiated_version = (
            requested_version if requested_version in SUPPORTED_PROTOCOL_VERSIONS
            else PROTOCOL_VERSION
        )
        result = {
            "protocolVersion": negotiated_version,
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "kor-motorforge", "version": SERVER_VERSION},
            "instructions": (
                "Use Motorforge tools for project creation and validation. "
                "Paths are restricted to the configured workspace unless explicitly allowed."
            ),
        }
        return {"jsonrpc": "2.0", "id": request_id, "result": result}

    if method == "ping":
        return {"jsonrpc": "2.0", "id": request_id, "result": {}}

    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}}

    if method == "tools/call":
        tool_name = params.get("name")
        arguments = params.get("arguments", {})
        if not isinstance(tool_name, str):
            return rpc_error(request_id, -32602, "tools/call requires a string tool name.")
        try:
            result = call_tool(tool_name, arguments, executable, workspace, allow_external)
            return {"jsonrpc": "2.0", "id": request_id, "result": result}
        except ToolFailure as error:
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "content": [{"type": "text", "text": json_text({
                        "ok": False,
                        "error": {"code": error.code, "message": error.message},
                    })}],
                    "isError": True,
                },
            }

    if method == "shutdown":
        return {"jsonrpc": "2.0", "id": request_id, "result": {}}

    return rpc_error(request_id, -32601, f"Method not found: {method}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    default_executable = Path(__file__).resolve().parents[1] / "build" / "motorforge"
    parser.add_argument(
        "--motorforge",
        type=Path,
        default=Path(os.environ.get("MOTORFORGE_EXECUTABLE", default_executable)),
        help="Path to the compiled Motorforge CLI (default: build/motorforge).",
    )
    parser.add_argument(
        "--workspace",
        type=Path,
        default=Path.cwd(),
        help="Root directory that project paths are restricted to (default: current directory).",
    )
    parser.add_argument(
        "--allow-external-paths",
        action="store_true",
        help="Allow tools to read/create projects outside the configured workspace.",
    )
    args = parser.parse_args()

    executable = args.motorforge.expanduser().resolve()
    workspace = args.workspace.expanduser().resolve()
    if not executable.is_file():
        print(f"Motorforge executable not found: {executable}", file=sys.stderr)
        return 2
    if not workspace.is_dir():
        print(f"Workspace directory does not exist: {workspace}", file=sys.stderr)
        return 2

    for line in sys.stdin:
        try:
            message = json.loads(line)
            response = handle_message(message, executable, workspace, args.allow_external_paths)
        except json.JSONDecodeError:
            response = rpc_error(None, -32700, "Parse error.")
        except Exception as error:  # Keep one malformed request from crashing the server.
            print(f"Unhandled MCP adapter error: {error}", file=sys.stderr)
            response = rpc_error(None, -32603, "Internal server error.")

        if response is not None:
            sys.stdout.write(json.dumps(response, ensure_ascii=False, separators=(",", ":")) + "\n")
            sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
