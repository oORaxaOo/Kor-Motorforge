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
                "arguments": {"path": "sample-project/assets/factory.svg", "seed": 42, "size": 64},
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
                    "sprite_asset": "assets/factory.svg",
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
        inspected_for_mutation = tool_payload(second[11])
        updated_batch = [
            request(13, "tools/call", {
                "name": "entity_update",
                "arguments": {
                    "project_path": "sample-project",
                    "scene_id": "main-scene",
                    "id": "factory",
                    "name": "Factory Updated",
                    "expected_revision": inspected_for_mutation["revision"],
                    "x": 21,
                    "y": 9,
                },
            }),
            request(14, "tools/call", {
                "name": "scene_inspect",
                "arguments": {"project_path": "sample-project", "scene_id": "main-scene"},
            }),
        ]
        updated_responses = run_server(server, args.motorforge, workspace, updated_batch)
        assert updated_responses[13]["result"]["isError"] is False
        updated = tool_payload(updated_responses[13])
        assert updated["result"]["entity"]["name"] == "Factory Updated"
        assert updated["result"]["entity"]["components"]["transform"]["x"] == 21
    
        runtime_batch = [
            request(17, "tools/call", {
                "name": "runtime_inspect",
                "arguments": {"project_path": "sample-project", "scene_id": "main-scene"},
            }),
        ]
        runtime_responses = run_server(server, args.motorforge, workspace, runtime_batch)
        runtime_payload = tool_payload(runtime_responses[17])
        assert runtime_responses[17]["result"]["isError"] is False
        assert runtime_payload["result"]["runtime"]["status"] == "ready"
        assert runtime_payload["result"]["sprite_entity_count"] == 1

        preview_batch = [
            request(18, "tools/call", {
                "name": "scene_preview_render",
                "arguments": {
                    "project_path": "sample-project", "scene_id": "main-scene",
                    "output_path": "sample-project/scene-preview.svg", "width": 640, "height": 480,
                },
            }),
        ]
        preview_responses = run_server(server, args.motorforge, workspace, preview_batch)
        preview_payload = tool_payload(preview_responses[18])
        assert preview_responses[18]["result"]["isError"] is False
        assert preview_payload["result"]["format"] == "svg"
        assert (workspace / "sample-project" / "scene-preview.svg").read_text(encoding="utf-8").startswith("<svg")

        interactive_batch = [
            request(19, "tools/call", {
                "name": "scene_preview_interactive",
                "arguments": {
                    "project_path": "sample-project", "scene_id": "main-scene",
                    "output_path": "sample-project/scene-viewer.html", "width": 640, "height": 480,
                },
            }),
        ]
        interactive_responses = run_server(server, args.motorforge, workspace, interactive_batch)
        interactive_payload = tool_payload(interactive_responses[19])
        assert interactive_responses[19]["result"]["isError"] is False
        assert interactive_payload["result"]["format"] == "html"
        viewer = (workspace / "sample-project" / "scene-viewer.html").read_text(encoding="utf-8")
        assert viewer.startswith("<!doctype html>")
        assert "zoomAt" in viewer and "pointermove" in viewer

        deleted_batch = [
            request(15, "tools/call", {
                "name": "entity_delete",
                "arguments": {
                    "project_path": "sample-project",
                    "scene_id": "main-scene",
                    "id": "factory",
                    "expected_revision": tool_payload(updated_responses[14])["revision"],
                },
            }),
            request(16, "tools/call", {
                "name": "scene_inspect",
                "arguments": {"project_path": "sample-project", "scene_id": "main-scene"},
            }),
        ]
        deleted_responses = run_server(server, args.motorforge, workspace, deleted_batch)
        assert deleted_responses[15]["result"]["isError"] is False
        assert tool_payload(deleted_responses[15])["result"]["deleted_entity_id"] == "factory"
        assert tool_payload(deleted_responses[16])["result"]["entities"] == []
        generated_svg = (workspace / "sample-project" / "assets" / "factory.svg").read_text(encoding="utf-8")

    initialized = first[1]["result"]
    assert initialized["serverInfo"]["name"] == "kor-motorforge"
    assert initialized["protocolVersion"] == "2025-11-25"

    tools = first[2]["result"]["tools"]
    tool_names = {tool["name"] for tool in tools}
    expected_tools = {
        "motorforge_capabilities", "validate_project", "create_project",
        "generate_procedural_sprite", "scene_list", "scene_create",
        "scene_inspect", "entity_create", "entity_update", "entity_delete", "runtime_inspect", "scene_preview_render", "scene_preview_interactive",
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
