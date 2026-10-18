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
    {
        "name": "generate_procedural_sprite",
        "description": "Generate a deterministic, editable SVG sprite from procedural rules without using an image-generation model.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "minLength": 1},
                "type": {"type": "string", "enum": ["industrial-building"]},
                "seed": {"type": "integer", "minimum": 0, "maximum": 4294967295},
                "size": {"type": "integer", "minimum": 16, "maximum": 1024},
            },
            "required": ["path"],
            "additionalProperties": False,
        },
    },
    {
        "name": "scene_list",
        "description": "List scenes in a project and return revision tokens.",
        "inputSchema": {
            "type": "object",
            "properties": {"project_path": {"type": "string", "minLength": 1}},
            "required": ["project_path"],
            "additionalProperties": False,
        },
    },
    {
        "name": "scene_create",
        "description": "Create a new scene in a project. Existing scene IDs are never overwritten.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "name": {"type": "string", "minLength": 1},
                "id": {"type": "string"},
            },
            "required": ["project_path", "name"],
            "additionalProperties": False,
        },
    },
    {
        "name": "scene_inspect",
        "description": "Inspect a scene and retrieve the revision token required for safe entity mutations.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1},
            },
            "required": ["project_path", "scene_id"],
            "additionalProperties": False,
        },
    },
    {
        "name": "entity_create",
        "description": "Create an entity with transform and optional sprite component. Requires the latest scene revision token.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1},
                "id": {"type": "string", "minLength": 1},
                "name": {"type": "string", "minLength": 1},
                "expected_revision": {"type": "string", "minLength": 1},
                "x": {"type": "number"},
                "y": {"type": "number"},
                "sprite_asset": {"type": "string"},
            },
            "required": ["project_path", "scene_id", "id", "name", "expected_revision"],
            "additionalProperties": False,
        },
    },
    {
        "name": "entity_update",
        "description": "Update an entity's name and position using the latest scene revision token.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1},
                "id": {"type": "string", "minLength": 1},
                "name": {"type": "string", "minLength": 1},
                "expected_revision": {"type": "string", "minLength": 1},
                "x": {"type": "number"},
                "y": {"type": "number"}
            },
            "required": ["project_path", "scene_id", "id", "name", "expected_revision", "x", "y"],
            "additionalProperties": False
        }
    },
    {
        "name": "scene_preview_render",
        "description": "Render a portable SVG preview of a scene with a dark grid, world axes, entity markers, and embedded SVG sprites.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1},
                "output_path": {"type": "string", "minLength": 1},
                "width": {"type": "integer", "minimum": 320, "maximum": 4096},
                "height": {"type": "integer", "minimum": 240, "maximum": 4096}
            },
            "required": ["project_path", "scene_id", "output_path"],
            "additionalProperties": False
        }
    },
    {
        "name": "runtime_inspect",
        "description": "Load a scene in headless mode, validate runtime transforms, and resolve sprite assets without opening a window.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1}
            },
            "required": ["project_path", "scene_id"],
            "additionalProperties": False
        }
    },
    {
        "name": "entity_delete",
        "description": "Delete an entity using the latest scene revision token.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project_path": {"type": "string", "minLength": 1},
                "scene_id": {"type": "string", "minLength": 1},
                "id": {"type": "string", "minLength": 1},
                "expected_revision": {"type": "string", "minLength": 1}
            },
            "required": ["project_path", "scene_id", "id", "expected_revision"],
            "additionalProperties": False
        }
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
    elif name == "generate_procedural_sprite":
        if set(arguments) - {"path", "type", "seed", "size"} or "path" not in arguments:
            raise ToolFailure("INVALID_ARGUMENTS", "generate_procedural_sprite requires path; type, seed, and size are optional.")
        path = resolve_workspace_path(arguments.get("path"), workspace, allow_external)
        sprite_type = arguments.get("type", "industrial-building")
        seed = arguments.get("seed", 1)
        size = arguments.get("size", 64)
        if sprite_type != "industrial-building":
            raise ToolFailure("UNSUPPORTED_SPRITE_TYPE", "Supported sprite type: industrial-building.")
        if isinstance(seed, bool) or not isinstance(seed, int) or seed < 0 or seed > 4294967295:
            raise ToolFailure("INVALID_SEED", "seed must be an unsigned 32-bit integer.")
        if isinstance(size, bool) or not isinstance(size, int) or size < 16 or size > 1024:
            raise ToolFailure("INVALID_SPRITE_SIZE", "size must be an integer between 16 and 1024.")
        return_code, output = invoke_motorforge(
            executable,
            ["spritegen", str(path), "--type", sprite_type, "--seed", str(seed), "--size", str(size), "--json"],
        )
    elif name in {"entity_update", "entity_delete"}:
        required = {"project_path", "scene_id", "id", "expected_revision"}
        allowed = required | ({"name", "x", "y"} if name == "entity_update" else set())
        if set(arguments) != allowed:
            raise ToolFailure("INVALID_ARGUMENTS", f"{name} requires exactly: {', '.join(sorted(allowed))}.")
        project_path = resolve_workspace_path(arguments["project_path"], workspace, allow_external)
        scene_id, entity_id, revision = arguments["scene_id"], arguments["id"], arguments["expected_revision"]
        if not all(isinstance(value, str) and value for value in (scene_id, entity_id, revision)):
            raise ToolFailure("INVALID_ARGUMENTS", "scene_id, id, and expected_revision must be non-empty strings.")
        if name == "entity_update":
            entity_name, x, y = arguments["name"], arguments["x"], arguments["y"]
            if not isinstance(entity_name, str) or not entity_name.strip():
                raise ToolFailure("INVALID_ENTITY_NAME", "name must be a non-empty string.")
            if (isinstance(x, bool) or not isinstance(x, (int, float)) or
                    isinstance(y, bool) or not isinstance(y, (int, float))):
                raise ToolFailure("INVALID_TRANSFORM", "x and y must be numbers.")
            return_code, output = invoke_motorforge(executable, [
                "entity", "update", str(project_path), scene_id, "--id", entity_id,
                "--name", entity_name, "--expected-revision", revision,
                "--x", str(x), "--y", str(y), "--json",
            ])
        else:
            return_code, output = invoke_motorforge(executable, [
                "entity", "delete", str(project_path), scene_id, "--id", entity_id,
                "--expected-revision", revision, "--json",
            ])
    elif name == "scene_preview_interactive":
        allowed = {"project_path", "scene_id", "output_path", "width", "height"}
        required = {"project_path", "scene_id", "output_path"}
        if not required <= set(arguments) or set(arguments) - allowed:
            raise ToolFailure("INVALID_ARGUMENTS", "scene_preview_interactive requires project_path, scene_id, and output_path; width and height are optional.")
        project_path = resolve_workspace_path(arguments["project_path"], workspace, allow_external)
        output_path = resolve_workspace_path(arguments["output_path"], workspace, allow_external)
        scene_id = arguments["scene_id"]
        if not isinstance(scene_id, str) or not scene_id:
            raise ToolFailure("INVALID_SCENE_ID", "scene_id must be a non-empty string.")
        command = ["preview", "interactive", str(project_path), scene_id, "--output", str(output_path), "--json"]
        for key in ("width", "height"):
            if key in arguments:
                if isinstance(arguments[key], bool) or not isinstance(arguments[key], int):
                    raise ToolFailure("INVALID_PREVIEW_SIZE", f"{key} must be an integer.")
                command.extend(["--" + key, str(arguments[key])])
        return_code, output = invoke_motorforge(executable, command)
    elif name == "scene_preview_render":
        allowed = {"project_path", "scene_id", "output_path", "width", "height"}
        required = {"project_path", "scene_id", "output_path"}
        if not required <= set(arguments) or set(arguments) - allowed:
            raise ToolFailure("INVALID_ARGUMENTS", "scene_preview_render requires project_path, scene_id, and output_path; width and height are optional.")
        project_path = resolve_workspace_path(arguments["project_path"], workspace, allow_external)
        output_path = resolve_workspace_path(arguments["output_path"], workspace, allow_external)
        scene_id = arguments["scene_id"]
        if not isinstance(scene_id, str) or not scene_id:
            raise ToolFailure("INVALID_SCENE_ID", "scene_id must be a non-empty string.")
        command = ["preview", "render", str(project_path), scene_id, "--output", str(output_path), "--json"]
        for key in ("width", "height"):
            if key in arguments:
                if isinstance(arguments[key], bool) or not isinstance(arguments[key], int):
                    raise ToolFailure("INVALID_PREVIEW_SIZE", f"{key} must be an integer.")
                command.extend(["--" + key, str(arguments[key])])
        return_code, output = invoke_motorforge(executable, command)
    elif name == "runtime_inspect":
        if set(arguments) != {"project_path", "scene_id"}:
            raise ToolFailure("INVALID_ARGUMENTS", "runtime_inspect requires project_path and scene_id.")
        project_path = resolve_workspace_path(arguments["project_path"], workspace, allow_external)
        scene_id = arguments["scene_id"]
        if not isinstance(scene_id, str) or not scene_id:
            raise ToolFailure("INVALID_SCENE_ID", "scene_id must be a non-empty string.")
        return_code, output = invoke_motorforge(
            executable, ["runtime", "inspect", str(project_path), scene_id, "--json"]
        )
    elif name in {"scene_list", "scene_create", "scene_inspect", "entity_create"}:
        project_path = resolve_workspace_path(arguments.get("project_path"), workspace, allow_external)
        if name == "scene_list":
            if set(arguments) != {"project_path"}:
                raise ToolFailure("INVALID_ARGUMENTS", "scene_list accepts only project_path.")
            command = ["scene", "list", str(project_path), "--json"]
        elif name == "scene_create":
            if set(arguments) - {"project_path", "name", "id"} or "name" not in arguments:
                raise ToolFailure("INVALID_ARGUMENTS", "scene_create requires project_path and name; id is optional.")
            scene_name = arguments.get("name")
            scene_id = arguments.get("id")
            if not isinstance(scene_name, str) or not scene_name.strip():
                raise ToolFailure("INVALID_SCENE_NAME", "name must be a non-empty string.")
            if scene_id is not None and not isinstance(scene_id, str):
                raise ToolFailure("INVALID_SCENE_ID", "id must be a string when provided.")
            command = ["scene", "create", str(project_path), "--name", scene_name, "--json"]
            if scene_id:
                command.extend(["--id", scene_id])
        elif name == "scene_inspect":
            if set(arguments) != {"project_path", "scene_id"}:
                raise ToolFailure("INVALID_ARGUMENTS", "scene_inspect requires project_path and scene_id.")
            scene_id = arguments.get("scene_id")
            if not isinstance(scene_id, str) or not scene_id:
                raise ToolFailure("INVALID_SCENE_ID", "scene_id must be a non-empty string.")
            command = ["scene", "inspect", str(project_path), scene_id, "--json"]
        else:
            allowed = {"project_path", "scene_id", "id", "name", "expected_revision", "x", "y", "sprite_asset"}
            required = {"project_path", "scene_id", "id", "name", "expected_revision"}
            if set(arguments) - allowed or not required <= set(arguments):
                raise ToolFailure("INVALID_ARGUMENTS", "entity_create requires project_path, scene_id, id, name, expected_revision; x, y, and sprite_asset are optional.")
            scene_id = arguments["scene_id"]
            entity_id = arguments["id"]
            entity_name = arguments["name"]
            expected_revision = arguments["expected_revision"]
            x = arguments.get("x", 0.0)
            y = arguments.get("y", 0.0)
            sprite_asset = arguments.get("sprite_asset", "")
            if not all(isinstance(value, str) and value for value in (scene_id, entity_id, entity_name, expected_revision)):
                raise ToolFailure("INVALID_ARGUMENTS", "scene_id, id, name, and expected_revision must be non-empty strings.")
            if isinstance(x, bool) or not isinstance(x, (int, float)) or isinstance(y, bool) or not isinstance(y, (int, float)):
                raise ToolFailure("INVALID_TRANSFORM", "x and y must be numbers.")
            if not isinstance(sprite_asset, str):
                raise ToolFailure("INVALID_ASSET_PATH", "sprite_asset must be a string.")
            command = [
                "entity", "create", str(project_path), scene_id,
                "--id", entity_id, "--name", entity_name,
                "--expected-revision", expected_revision,
                "--x", str(x), "--y", str(y), "--json",
            ]
            if sprite_asset:
                command.extend(["--sprite", sprite_asset])
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
            requested_version if isinstance(requested_version, str) and requested_version in SUPPORTED_PROTOCOL_VERSIONS
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
