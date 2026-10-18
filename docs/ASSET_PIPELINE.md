# AI Asset and Animation Contract

Kor Motorforge is an agent-operated engine. Asset workflows must be callable through CLI/MCP, deterministic after source images exist, inspectable as JSON, and safe to rerun. A human-facing asset editor is optional.

## Animation manifest v1

The schema in `schemas/animation.schema.json` describes an animation asset independently of any specific renderer. It references a project-relative atlas image and defines frame dimensions, named clips, playback rate, playback mode, anchor/pivot, optional per-frame dimensions/durations/events, direction names, and provenance.

Example:

```json
{
  "schema_version": 1,
  "id": "kor-heavy-tank",
  "source": "assets/units/kor-heavy-tank-atlas.png",
  "license": "project-owned",
  "provenance": {
    "kind": "ai-generated",
    "generator": "image-provider-name",
    "model": "model-version",
    "prompt": "Heavy industrial RTS tank, consistent top-down 8-direction sprite",
    "seed": 4815
  },
  "frame_size": { "width": 96, "height": 96 },
  "atlas": { "columns": 8, "rows": 4, "spacing": 0, "margin": 0 },
  "directions": ["north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"],
  "clips": [
    {
      "id": "idle",
      "loop": true,
      "fps": 8,
      "playback": "forward",
      "anchor": { "x": 48, "y": 72 },
      "frames": [
        { "x": 0, "y": 0 },
        { "x": 96, "y": 0, "event": "idle_breath" }
      ]
    },
    {
      "id": "fire",
      "loop": false,
      "fps": 16,
      "frames": [
        { "x": 0, "y": 96, "duration_ms": 80 },
        { "x": 96, "y": 96, "duration_ms": 80, "event": "muzzle_flash" }
      ]
    }
  ],
  "tags": ["unit", "vehicle", "kor", "rts"]
}
```

The example is illustrative; the manifest schema validates shape, not the actual image file or semantic visual consistency.

## Coordinate and playback rules

- Atlas frame coordinates are pixel coordinates from the top-left corner.
- A frame's optional width/height override the manifest's frame size.
- If `duration_ms` is absent, a runtime may derive frame duration from clip FPS; if both are present, frame duration wins.
- Anchor coordinates are relative to a frame's top-left corner, in pixels.
- `directions` records available orientations but does not imply that a generator successfully produced visually consistent views.
- Frame events are identifiers, not executable scripts. Runtime event dispatch must use an allowlisted event system.
- Asset paths are project-relative identifiers. Consumers must canonicalize paths and reject traversal or symlink escapes outside the project asset root.
- Provenance is optional metadata, not proof of ownership or license. A production pipeline should require a license/provenance policy before shipping.

## Required next implementation steps

1. Validate manifests against the schema and validate atlas bounds against the actual image dimensions.
2. Add image metadata probing and deterministic frame slicing/atlas utilities.
3. Add CLI and MCP tools that return structured diagnostics and stable error codes.
4. Add an animation inspection/preview tool that can be exercised in headless CI.
5. Keep image-model providers pluggable: generation is stochastic/external; asset normalization and validation should be deterministic.

Do not report an animation as usable solely because its JSON is valid. Validation must eventually confirm that the referenced image exists, all frame rectangles fit within the atlas, and all required runtime constraints are met.
