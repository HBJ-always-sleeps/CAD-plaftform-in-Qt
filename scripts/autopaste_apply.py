# -*- coding: utf-8 -*-
"""Write batch-paste results with ezdxf so the output remains CAD-compatible."""

import json
import sys
from pathlib import Path

import ezdxf


def main() -> int:
    if len(sys.argv) != 2:
        print("[ERROR] Usage: autopaste_apply.py <input.json>")
        return 2

    config_path = Path(sys.argv[1])
    with config_path.open("r", encoding="utf-8") as stream:
        config = json.load(stream)

    source_path = Path(config["destination_dxf"])
    output_path = Path(config["output_dxf"])
    layer_name = str(config["output_layer"])
    polylines = config.get("polylines", [])

    doc = ezdxf.readfile(source_path)
    if layer_name not in doc.layers:
        doc.layers.new(name=layer_name, dxfattribs={"color": 3})
    else:
        doc.layers.get(layer_name).dxf.color = 3

    modelspace = doc.modelspace()
    written = 0
    for points in polylines:
        if not isinstance(points, list) or len(points) < 2:
            continue
        modelspace.add_lwpolyline(
            [(float(point[0]), float(point[1])) for point in points],
            dxfattribs={"layer": layer_name, "color": 3},
        )
        written += 1

    output_path.parent.mkdir(parents=True, exist_ok=True)
    doc.saveas(output_path)

    # Reopen with ezdxf to reject a partially written DXF before C++ returns it.
    verification_doc = ezdxf.readfile(output_path)
    verified = sum(
        1 for entity in verification_doc.modelspace().query("LWPOLYLINE")
        if entity.dxf.layer == layer_name
    )
    if verified < written:
        raise RuntimeError(f"layer verification failed: expected {written}, got {verified}")

    print(json.dumps({"written": written, "verified": verified}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"[ERROR] {error}")
        raise
