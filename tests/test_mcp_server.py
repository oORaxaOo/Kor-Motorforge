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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--motorforge", required=True, type=Path)
    args = parser.parse_args()

    server = Path(__file__).resolve().parents[1] / "tools" / "mcp_server.py"
    with tempfile.TemporaryDirectory(prefix="motorforge-mcp-test-") as temporary:
        workspace = Path(temporary)
        messages = [
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
                "name": "generate_procedural_sprite",
                "arguments": {"path": "sprites/factory.svg", "seed": 42, "size": 64},
            }),
            request(6, "tools/call", {
                "name": "validate_project",
                "arguments": {"path": "../outside-project"},
            }),
        ]
        completed = subprocess.run(
            [
                sys.executable, str(server),
                "--motorforge", str(args.motorforge.resolve()),
                "--workspace", str(workspace),
            ],
            input="\n".join(json.dumps(message) for message in messages) + "\n",
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        generated_svg = (workspace / "sprites" / "factory.svg").read_text(encoding="utf-8")

    if completed.returncode != 0:
        raise AssertionError(f"MCP server exited {completed.returncode}: {completed.stderr}")

    responses = [json.loads(line) for line in completed.stdout.splitlines() if line.strip()]
    by_id = {response["id"]: response for response in responses}
    assert set(by_id) == {1, 2, 3, 4, 5, 6}, f"Unexpected MCP responses: {responses}"

    initialized = by_id[1]["result"]
    assert initialized["serverInfo"]["name"] == "kor-motorforge"
    assert initialized["protocolVersion"] == "2025-11-25"

    tools = by_id[2]["result"]["tools"]
    tool_names = {tool["name"] for tool in tools}
    assert {"motorforge_capabilities", "validate_project", "create_project", "generate_procedural_sprite"} <= tool_names

    created = json.loads(by_id[3]["result"]["content"][0]["text"])
    assert created["ok"] is True
    assert created["project"]["id"] == "mcp-sample-project"

    validated = json.loads(by_id[4]["result"]["content"][0]["text"])
    assert validated["ok"] is True

    generated = json.loads(by_id[5]["result"]["content"][0]["text"])
    assert generated["ok"] is True
    assert generated["asset"]["seed"] == 42
    assert generated_svg.startswith("<svg ")
    assert "generator=industrial-building-v1; seed=42" in generated_svg

    escaped = by_id[6]["result"]
    assert escaped["isError"] is True
    escaped_payload = json.loads(escaped["content"][0]["text"])
    assert escaped_payload["error"]["code"] == "PATH_OUTSIDE_WORKSPACE"

    print("MCP adapter integration tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
