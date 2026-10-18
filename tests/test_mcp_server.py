#!/usr/bin/env python3
"""Integration tests for the stdio MCP adapter using the real Motorforge CLI."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def request(request_id: int, method: str, params: dict | None = None) -> dict:
    return {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": method,
        "params": params or {},
    }


def run_server(server: Path, executable: Path, workspace: Path, messages: list[dict]) -> dict[int, dict]:
    completed = subprocess.run(
        [
            sys.executable, str(server),
            "--motorforge", str(executable.resolve()),
            "--workspace", str(workspace),
        ],
        input="\n".join(json.dumps(message) for message in messages) + "\n",
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(f"MCP server exited {completed.returncode}: {completed.stderr}")
    responses = [json.loads(line) for line in completed.stdout.splitlines() if line.strip()]
    return {response["id"]: response for response in responses}


def tool_payload(response: dict) -> dict:
    return json.loads(response["result"]["content"][0]["text"])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--motorforge", required=True, type=Path)
    args = parser.parse_args()

    server = Path(__file__).resolve().parents[1] / "tools" / "mcp_server.py"
    with tempfile.TemporaryDirectory(prefix="motorforge-mcp-test-") as temporary:
        workspace = Path(temporary)
        first_batch = [
            request(1, "initialize", {
                "protocolVersion": "2025-11-25",
                "capabilities": {},
                "clientInfo": {"name": "motorforge-test-client", "version": "1"},
            }),
            {"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}},
            request(2, "tools/list"),
            request(3, "tools/call", {
                "name": "create_project",
                "arguments": {"path": "sample-project", "name": "MCP Sample Project"},
            }),
            request(4, "tools/call", {
                "name": "validate_project",
                "arguments": {"path": "sample-project"},
            }),
            request(5, "tools/call", {
                "name": "scene_create",
                "arguments": {"project_path": "sample-project", "name": "Main Scene"},
            }),
            request(6, "tools/call", {
                "name": "scene_inspect",
                "arguments": {"project_path": "sample-project", "scene_id": "main-scene"},
            }),
            request(7, "tools/call", {
                "name": "generate_procedural_sprite",
                "arguments": {"path": "sprites/factory.svg", "seed": 42, "size": 64},
            }),
            request(8, "tools/call", {
                "name": "validate_project",
                "arguments": {"path": "../outside-project"},
            }),
        ]
        first = run_server(server, args.motorforge, workspace, first_batch)
        assert set(first) == {1, 2, 3, 4, 5, 6, 7, 8}, f"Unexpected first MCP responses: {first}"

        scene_revision = tool_payload(first[6])["revision"]
        second_batch = [
            request(9, "tools/call", {
                "name": "entity_create",
                "arguments": {
                    "project_path": "sample-project",
                    "scene_id": "main-scene",
                    "id": "factory",
                    "name": "Factory",
                    "expected_revision": scene_revision,
                    "x": 12.5,
                    "y": 7,
                    "sprite_asset": "sprites/factory.svg",
                },
            }),
            request(10, "tools/call", {
                "name": "entity_create",
                "arguments": {
                    "project_path": "sample-project",
                    "scene_id": "main-scene",
                    "id": "stale-entity",
                    "name": "Stale Entity",
                    "expected_revision": scene_revision,
                },
            }),
            request(11, "tools/call", {
                "name": "scene_inspect",
                "arguments": {"project_path": "sample-project", "scene_id": "main-scene"},
            }),
            request(12, "tools/call", {
                "name": "scene_list",
                "arguments": {"project_path": "sample-project"},
            }),
        ]
        second = run_server(server, args.motorforge, workspace, second_batch)
        generated_svg = (workspace / "sprites" / "factory.svg").read_text(encoding="utf-8")

    initialized = first[1]["result"]
    assert initialized["serverInfo"]["name"] == "kor-motorforge"
    assert initialized["protocolVersion"] == "2025-11-25"

    tools = first[2]["result"]["tools"]
    tool_names = {tool["name"] for tool in tools}
    expected_tools = {
        "motorforge_capabilities", "validate_project", "create_project",
        "generate_procedural_sprite", "scene_list", "scene_create",
        "scene_inspect", "entity_create",
    }
    assert expected_tools <= tool_names

    created = tool_payload(first[3])
    assert created["ok"] is True
    assert created["project"]["id"] == "mcp-sample-project"
    assert tool_payload(first[4])["ok"] is True

    scene_created = tool_payload(first[5])
    assert scene_created["ok"] is True
    assert scene_created["result"]["scene"]["id"] == "main-scene"
    assert tool_payload(first[6])["revision"] == scene_revision

    generated = tool_payload(first[7])
    assert generated["ok"] is True
    assert generated["asset"]["seed"] == 42
    assert generated_svg.startswith("<svg ")
    assert "generator=industrial-building-v1; seed=42" in generated_svg

    escaped = tool_payload(first[8])
    assert escaped["error"]["code"] == "PATH_OUTSIDE_WORKSPACE"

    assert second[9]["result"]["isError"] is False
    entity_result = tool_payload(second[9])
    assert entity_result["ok"] is True
    assert entity_result["result"]["entity"]["id"] == "factory"

    assert second[10]["result"]["isError"] is True
    stale_result = tool_payload(second[10])
    assert stale_result["errors"][0]["code"] == "REVISION_CONFLICT"

    inspected = tool_payload(second[11])
    assert inspected["ok"] is True
    assert len(inspected["result"]["entities"]) == 1
    listed = tool_payload(second[12])
    assert len(listed["result"]["scenes"]) == 1

    print("MCP adapter integration tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
