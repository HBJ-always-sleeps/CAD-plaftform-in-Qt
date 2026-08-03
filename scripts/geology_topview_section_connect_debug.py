# -*- coding: utf-8 -*-
"""Verified v4.1 core: connect real blocks section by section.

This is shared by HydraulicCADPlatform and independent diagnostic runs.

Key rules:
1. DMX-to-station assignment exactly follows the Qt platform's stratified
   quantity workflow (StationMatcher + DMX bounding-box centre).
2. Material HATCHes are collected by the matched DMX section box, never by
   nearest station-text distance.
3. Every section keeps its separate 2.5 m transverse blocks.
4. A block connects only to a similar block on the immediately adjacent
   25 m section.  Missing blocks create a real break.
5. First/last appearances receive half-section caps and cannot disappear.
6. All station lines use one common length, determined by the widest result,
   and are labelled at both ends.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

import ezdxf
import numpy as np
from ezdxf.path import from_hatch, group_paths
from shapely.geometry import (
    GeometryCollection,
    LineString,
    MultiPolygon,
    Point,
    Polygon,
    box,
)
from shapely.ops import unary_union


HORIZONTAL_SCALE = 3.0
VERTICAL_SCALE = 0.2
SECTION_INTERVAL = 25
TRANSVERSE_GRID = 2.5
HALF_SECTION = SECTION_INTERVAL / 2.0
SECTION_BOX_X_MARGIN = 20.0
SECTION_BOX_Y_MARGIN = 50.0
STATION_MATCH_TOLERANCE = 500.0
BLOCK_MATCH_MAX_CENTER_DISTANCE = 60.0
BLOCK_MATCH_MAX_EDGE_GAP = 20.0
STATION_LINE_MARGIN = 20.0
STATION_TEXT_MARGIN = 8.0
UPDATE_SECTION_LAYER = "0-已粘贴断面"

CATEGORY_CONFIG = {
    "MUD": {
        "layer": "TOPVIEW_MUD",
        "color": 8,
        "name": "淤泥（含填土）",
    },
    "CLAY": {
        "layer": "TOPVIEW_CLAY",
        "color": 30,
        "name": "黏土",
    },
    "SAND": {
        "layer": "TOPVIEW_SAND",
        "color": 2,
        "name": "砂（含碎石）",
    },
    "BACKFILL": {
        "layer": "TOPVIEW_BACKFILL",
        "color": 4,
        "name": "回淤",
    },
}

# Input materials remain four categories.  The output splits only the three
# geological materials into five thickness bands; backfill keeps its original
# single layer and filtering logic.
SOURCE_CATEGORY_CONFIG = CATEGORY_CONFIG
THICKNESS_BANDS = (
    ("0_0_5", "0-0.5m", 0.0, 0.5),
    ("0_5_1", "0.5-1m", 0.5, 1.0),
    ("1_1_5", "1-1.5m", 1.0, 1.5),
    ("1_5_2", "1.5-2m", 1.5, 2.0),
    ("over_2", "over-2m", 2.0, math.inf),
)

CATEGORY_CONFIG = {
    "BACKFILL": SOURCE_CATEGORY_CONFIG["BACKFILL"],
}
for _material in ("MUD", "CLAY", "SAND"):
    _base = SOURCE_CATEGORY_CONFIG[_material]
    # Parent layer: all valid thicknesses for this material, retained for
    # total-area display alongside its mutually exclusive thickness children.
    CATEGORY_CONFIG[_material] = _base
    for _band_key, _band_name, _, _ in THICKNESS_BANDS:
        _display_key = f"{_material}_{_band_key}"
        CATEGORY_CONFIG[_display_key] = {
            "layer": f"{_base['layer']}_T{_band_key}",
            "color": _base["color"],
            "name": f"{_base['name']} {_band_name}",
            "material": _material,
            "band": _band_key,
        }


STATION_PATTERN = re.compile(r"K?(\d+)\+(\d+)", re.IGNORECASE)


def log(message: str) -> None:
    print(message, flush=True)


def station_name(station: int) -> str:
    return f"K{station // 1000}+{station % 1000:03d}"


def parse_station(text: str) -> int | None:
    match = STATION_PATTERN.search(text or "")
    if not match:
        return None
    return int(match.group(1)) * 1000 + int(match.group(2))


def category_for_layer(layer_name: str) -> str | None:
    # The combined quantity output keeps the original strata HATCHes and adds
    # calculated HATCHes named as follows:
    #   分层线_全算量_1级淤泥
    #   分层线_全算量_1级淤泥_设计
    #   分层线_全算量_1级淤泥_超挖
    # Only these calculated layers (or the legacy Nonem layers) may feed the
    # top view; otherwise source HATCHes would be counted a second time.
    if "\u56de\u6de4" in layer_name:
        return "BACKFILL"
    if not (
        layer_name.startswith("Nonem_")
        or layer_name.startswith("\u5206\u5c42\u7ebf_")
    ):
        return None
    if "\u6de4\u6ce5" in layer_name or "\u586b\u571f" in layer_name:
        return "MUD"
    if "\u9ecf\u571f" in layer_name:
        return "CLAY"
    if "\u7802" in layer_name or "\u788e\u77f3" in layer_name:
        return "SAND"
    return None


def load_spine_data(path: str) -> dict[int, dict[str, float]]:
    with open(path, "r", encoding="utf-8") as stream:
        data = json.load(stream)
    matches = data.get("matches", [])
    if not matches:
        raise ValueError("脊梁点JSON中没有matches数据")

    result = {}
    for match in matches:
        station = int(round(float(match["station_value"])))
        result[station] = {
            "spine_x": float(match["spine_x"]),
            "spine_y": float(match["spine_y"]),
            "l1_x": float(match["l1_x"]),
            "l1_y": float(match["l1_y"]),
            "tangent_angle": float(match["tangent_angle"]),
        }
    return result


def entity_bounds(points):
    xs = [float(point[0]) for point in points]
    ys = [float(point[1]) for point in points]
    return min(xs), min(ys), max(xs), max(ys)


def extract_station_texts(modelspace):
    stations = []
    for entity in modelspace:
        if entity.dxftype() != "TEXT":
            continue
        if "0-桩号" not in entity.dxf.layer:
            continue
        value = parse_station(entity.dxf.text)
        if value is None:
            continue
        stations.append({
            "text": entity.dxf.text,
            "station": value,
            "x": float(entity.dxf.insert.x),
            "y": float(entity.dxf.insert.y),
        })
    stations.sort(key=lambda item: item["y"], reverse=True)
    return stations


def extract_dmx_sections(modelspace):
    sections = []
    for entity in modelspace:
        if entity.dxftype() != "LWPOLYLINE":
            continue
        if entity.dxf.layer != "DMX":
            continue
        points = [
            (float(point[0]), float(point[1]))
            for point in entity.get_points("xy")
        ]
        if len(points) < 2:
            continue
        min_x, min_y, max_x, max_y = entity_bounds(points)
        sections.append({
            "points": points,
            "min_x": min_x,
            "min_y": min_y,
            "max_x": max_x,
            "max_y": max_y,
            "center_x": (min_x + max_x) / 2.0,
            "center_y": (min_y + max_y) / 2.0,
        })
    sections.sort(key=lambda item: item["center_y"], reverse=True)
    return sections


def polyline_y_at_x(points, x_value):
    candidates = []
    for (x1, y1), (x2, y2) in zip(points, points[1:]):
        low_x = min(x1, x2)
        high_x = max(x1, x2)
        if x_value < low_x - 1e-9 or x_value > high_x + 1e-9:
            continue
        if abs(x2 - x1) <= 1e-9:
            if abs(x_value - x1) <= 1e-9:
                candidates.extend((y1, y2))
            continue
        ratio = (x_value - x1) / (x2 - x1)
        candidates.append(y1 + ratio * (y2 - y1))
    return max(candidates) if candidates else None


def audit_upstream_backfill_lines(modelspace, sections):
    """Audit the source line stage used to create backfill HATCHes."""
    update_lines = []
    for entity in modelspace:
        if entity.dxftype() != "LWPOLYLINE":
            continue
        if entity.dxf.layer != UPDATE_SECTION_LAYER:
            continue
        points = [
            (float(point[0]), float(point[1]))
            for point in entity.get_points("xy")
        ]
        if len(points) >= 2:
            update_lines.append(points)

    assigned = {
        section["station"]: []
        for section in sections
    }
    for points in update_lines:
        min_x, min_y, max_x, max_y = entity_bounds(points)
        mid_x = (min_x + max_x) / 2.0
        mid_y = (min_y + max_y) / 2.0
        nearest = min(
            sections,
            key=lambda section: (
                (mid_x - section["center_x"]) ** 2
                + (mid_y - section["center_y"]) ** 2
            ),
        )
        assigned[nearest["station"]].append(points)

    audit = {}
    for section in sections:
        station = section["station"]
        lines = assigned[station]
        min_v = (
            section["ref_x"] - section["max_x"]
        ) * HORIZONTAL_SCALE
        max_v = (
            section["ref_x"] - section["min_x"]
        ) * HORIZONTAL_SCALE
        first_index = math.floor(min_v / TRANSVERSE_GRID)
        last_index = math.ceil(max_v / TRANSVERSE_GRID)
        signed_thicknesses = []
        for index in range(first_index, last_index + 1):
            centre_v = index * TRANSVERSE_GRID
            cad_x = (
                section["ref_x"]
                - centre_v / HORIZONTAL_SCALE
            )
            dmx_y = polyline_y_at_x(
                section["points"], cad_x
            )
            update_values = [
                polyline_y_at_x(points, cad_x)
                for points in lines
            ]
            update_values = [
                value for value in update_values
                if value is not None
            ]
            if dmx_y is None or not update_values:
                continue
            signed_thicknesses.append(
                (max(update_values) - dmx_y) * VERTICAL_SCALE
            )
        positive = [
            thickness
            for thickness in signed_thicknesses
            if thickness > 1e-9
        ]
        audit[station] = {
            "updateLineCount": len(lines),
            "sampleCount": len(signed_thicknesses),
            "positiveGapSampleCount": len(positive),
            "maxPositiveThicknessM": max(positive, default=0.0),
            "minSignedThicknessM": min(
                signed_thicknesses, default=0.0
            ),
            "maxSignedThicknessM": max(
                signed_thicknesses, default=0.0
            ),
        }
    return audit, len(update_lines)


def match_sections_like_platform(modelspace, spine_data):
    """Exact Python equivalent of EngineCad + StationMatcher."""
    station_texts = extract_station_texts(modelspace)
    dmx_sections = extract_dmx_sections(modelspace)
    if not station_texts:
        raise ValueError("没有找到0-桩号图层文字")
    if not dmx_sections:
        raise ValueError("没有找到DMX断面线")

    used_texts = set()
    matched = []
    for section in dmx_sections:
        best = None
        best_distance = float("inf")
        for station in station_texts:
            if station["text"] in used_texts:
                continue
            distance = math.sqrt(
                (station["x"] - section["center_x"]) ** 2 * 0.5
                + (station["y"] - section["center_y"]) ** 2
            )
            if (
                distance < best_distance
                and distance < STATION_MATCH_TOLERANCE
            ):
                best = station
                best_distance = distance

        if best is None:
            raise ValueError(
                "DMX断面无法匹配桩号: "
                f"中心({section['center_x']:.2f},"
                f"{section['center_y']:.2f})"
            )
        used_texts.add(best["text"])
        station_value = int(best["station"])
        if station_value not in spine_data:
            raise ValueError(
                f"{station_name(station_value)}不在脊梁点JSON中"
            )
        section = dict(section)
        section["station"] = station_value
        section["station_text"] = best["text"]
        section["match_distance"] = best_distance
        source_l1_x = float(spine_data[station_value]["l1_x"])
        section["source_l1_x"] = source_l1_x
        if (
            section["min_x"] - 1e-6
            <= source_l1_x
            <= section["max_x"] + 1e-6
        ):
            section["ref_x"] = source_l1_x
            section["ref_x_corrected"] = False
        else:
            # The spine JSON contains four known column-shifted L1 references
            # in the K72+625--K72+775 range.  Platform DMX matching has already
            # identified the real section, so an L1 point outside that DMX
            # cannot be the local horizontal origin.
            section["ref_x"] = section["center_x"]
            section["ref_x_corrected"] = True
        section["clip_box"] = box(
            section["min_x"] - SECTION_BOX_X_MARGIN,
            section["min_y"] - SECTION_BOX_Y_MARGIN,
            section["max_x"] + SECTION_BOX_X_MARGIN,
            section["max_y"] + SECTION_BOX_Y_MARGIN,
        )
        matched.append(section)

    matched.sort(key=lambda item: item["station"])
    station_values = [item["station"] for item in matched]
    duplicates = sorted({
        value for value in station_values
        if station_values.count(value) > 1
    })
    if duplicates:
        raise ValueError(
            "断面重复匹配桩号: "
            + ", ".join(station_name(value) for value in duplicates)
        )
    if len(matched) != len(spine_data):
        raise ValueError(
            f"平台断面匹配数{len(matched)}与脊梁点数"
            f"{len(spine_data)}不一致"
        )
    return matched


def polygon_parts(geometry):
    if geometry is None or geometry.is_empty:
        return []
    if isinstance(geometry, Polygon):
        return [geometry]
    if isinstance(geometry, MultiPolygon):
        return [
            polygon for polygon in geometry.geoms
            if not polygon.is_empty
        ]
    if isinstance(geometry, GeometryCollection):
        result = []
        for child in geometry.geoms:
            result.extend(polygon_parts(child))
        return result
    return []


def hatch_polygons(hatch):
    result = []
    try:
        paths = list(from_hatch(hatch))
        for path_group in group_paths(paths):
            if not path_group:
                continue
            exterior = [
                (float(vertex.x), float(vertex.y))
                for vertex in path_group[0].flattening(0.01)
            ]
            if len(exterior) < 3:
                continue
            holes = []
            for hole_path in path_group[1:]:
                hole = [
                    (float(vertex.x), float(vertex.y))
                    for vertex in hole_path.flattening(0.01)
                ]
                if len(hole) >= 3:
                    holes.append(hole)
            polygon = Polygon(exterior, holes)
            if not polygon.is_valid:
                polygon = polygon.buffer(0)
            result.extend(polygon_parts(polygon))
    except Exception:
        return []
    return result


def load_material_polygons(modelspace):
    records = []
    source_counts = defaultdict(int)
    source_layers = defaultdict(set)
    for entity in modelspace:
        if entity.dxftype() != "HATCH":
            continue
        layer_name = entity.dxf.layer
        category = category_for_layer(layer_name)
        if category is None:
            continue
        polygons = hatch_polygons(entity)
        if not polygons:
            continue
        source_counts[category] += 1
        source_layers[category].add(layer_name)
        for polygon in polygons:
            records.append({
                "category": category,
                "layer": layer_name,
                "polygon": polygon,
                "bounds": polygon.bounds,
            })
    return records, source_counts, source_layers


def bounds_overlap(a, b):
    return not (
        a[2] < b[0]
        or a[0] > b[2]
        or a[3] < b[1]
        or a[1] > b[3]
    )


def merge_intervals(intervals, max_gap=0.0):
    cleaned = sorted(
        (float(left), float(right))
        for left, right in intervals
        if right - left > 1e-8
    )
    if not cleaned:
        return []
    merged = [list(cleaned[0])]
    for left, right in cleaned[1:]:
        if left <= merged[-1][1] + max_gap:
            merged[-1][1] = max(merged[-1][1], right)
        else:
            merged.append([left, right])
    return [tuple(interval) for interval in merged]


def snap_intervals_to_grid(intervals):
    snapped = []
    for left, right in intervals:
        snapped_left = math.floor(
            (left + 1e-9) / TRANSVERSE_GRID
        ) * TRANSVERSE_GRID
        snapped_right = math.ceil(
            (right - 1e-9) / TRANSVERSE_GRID
        ) * TRANSVERSE_GRID
        if snapped_right <= snapped_left:
            snapped_right = snapped_left + TRANSVERSE_GRID
        snapped.append((snapped_left, snapped_right))
    return merge_intervals(snapped, max_gap=1e-8)


def max_linear_length(geometry):
    if geometry is None or geometry.is_empty:
        return 0.0
    if isinstance(geometry, LineString):
        return float(geometry.length)
    if hasattr(geometry, "geoms"):
        return max(
            (max_linear_length(child) for child in geometry.geoms),
            default=0.0,
        )
    return 0.0


def sample_backfill_thicknesses(geometry, ref_x):
    if geometry is None or geometry.is_empty:
        return []
    min_x, min_y, max_x, max_y = geometry.bounds
    min_v = (ref_x - max_x) * HORIZONTAL_SCALE
    max_v = (ref_x - min_x) * HORIZONTAL_SCALE
    first_index = math.floor(min_v / TRANSVERSE_GRID) - 1
    last_index = math.ceil(max_v / TRANSVERSE_GRID) + 1
    samples = []
    for index in range(first_index, last_index + 1):
        centre_v = index * TRANSVERSE_GRID
        cad_x = ref_x - centre_v / HORIZONTAL_SCALE
        if cad_x < min_x - 1e-9 or cad_x > max_x + 1e-9:
            continue
        section = geometry.intersection(LineString([
            (cad_x, min_y - 1.0),
            (cad_x, max_y + 1.0),
        ]))
        thickness = max_linear_length(section) * VERTICAL_SCALE
        if thickness <= 1e-9:
            continue
        samples.append((centre_v, thickness))
    return samples


def backfill_intervals_from_samples(
    samples,
    min_thickness,
    max_thickness,
):
    valid_centres = []
    for centre_v, thickness in samples:
        if (
            min_thickness is not None
            and thickness + 1e-9 < min_thickness
        ):
            continue
        if (
            max_thickness is not None
            and thickness - 1e-9 > max_thickness
        ):
            continue
        if thickness > 1e-9:
            valid_centres.append(centre_v)

    if not valid_centres:
        return []
    intervals = []
    start = valid_centres[0]
    previous = valid_centres[0]
    for centre in valid_centres[1:]:
        if abs(centre - previous - TRANSVERSE_GRID) <= 1e-8:
            previous = centre
            continue
        intervals.append((
            start - TRANSVERSE_GRID / 2.0,
            previous + TRANSVERSE_GRID / 2.0,
        ))
        start = centre
        previous = centre
    intervals.append((
        start - TRANSVERSE_GRID / 2.0,
        previous + TRANSVERSE_GRID / 2.0,
    ))
    return intervals


def intervals_from_centres(centres):
    """Turn adjacent transverse-grid centres into unmerged display blocks."""
    if not centres:
        return []
    centres = sorted(centres)
    intervals = []
    start = centres[0]
    previous = centres[0]
    for centre in centres[1:]:
        if abs(centre - previous - TRANSVERSE_GRID) <= 1e-8:
            previous = centre
            continue
        intervals.append((
            start - TRANSVERSE_GRID / 2.0,
            previous + TRANSVERSE_GRID / 2.0,
        ))
        start = centre
        previous = centre
    intervals.append((
        start - TRANSVERSE_GRID / 2.0,
        previous + TRANSVERSE_GRID / 2.0,
    ))
    return intervals


def thickness_band_key(thickness):
    for band_key, _, lower, upper in THICKNESS_BANDS:
        if thickness + 1e-9 >= lower and thickness < upper - 1e-9:
            return band_key
    return "over_2"


def backfill_intervals_from_geometry(
    geometry,
    ref_x,
    min_thickness,
    max_thickness,
):
    return backfill_intervals_from_samples(
        sample_backfill_thicknesses(geometry, ref_x),
        min_thickness,
        max_thickness,
    )


def extract_section_blocks(
    sections,
    material_records,
    spine_data,
    min_backfill_thickness,
    max_backfill_thickness,
):
    blocks = {
        key: {section["station"]: [] for section in sections}
        for key in CATEGORY_CONFIG
    }
    assigned_piece_counts = defaultdict(int)
    backfill_audit = {}

    for section in sections:
        station = section["station"]
        clip_geometry = section["clip_box"]
        clip_bounds = clip_geometry.bounds
        ref_x = section["ref_x"]
        backfill_parts = []
        geology_parts = defaultdict(list)

        for record in material_records:
            if not bounds_overlap(record["bounds"], clip_bounds):
                continue
            intersection = record["polygon"].intersection(clip_geometry)
            if intersection.is_empty:
                continue
            parts = polygon_parts(intersection)
            if not parts:
                continue
            category = record["category"]
            assigned_piece_counts[category] += len(parts)
            if category == "BACKFILL":
                backfill_parts.extend(parts)
                continue
            geology_parts[category].extend(parts)

        # Assign every 2.5 m transverse sample to one material-thickness
        # layer.  Therefore bands do not overlap or get merged across their
        # thickness boundary, while the physical colour stays unchanged.
        for category in ("MUD", "CLAY", "SAND"):
            if not geology_parts[category]:
                continue
            geology_geometry = unary_union(geology_parts[category])
            samples = sample_backfill_thicknesses(geology_geometry, ref_x)
            band_centres = defaultdict(list)
            all_centres = []
            for centre_v, thickness in samples:
                all_centres.append(centre_v)
                band_centres[thickness_band_key(thickness)].append(centre_v)
            blocks[category][station] = intervals_from_centres(all_centres)
            for band_key, _, _, _ in THICKNESS_BANDS:
                display_key = f"{category}_{band_key}"
                blocks[display_key][station] = intervals_from_centres(
                    band_centres[band_key]
                )

        if backfill_parts:
            backfill_geometry = unary_union(backfill_parts)
            samples = sample_backfill_thicknesses(
                backfill_geometry,
                ref_x,
            )
            filtered_intervals = backfill_intervals_from_samples(
                samples,
                min_backfill_thickness,
                max_backfill_thickness,
            )
            blocks["BACKFILL"][station] = filtered_intervals
            passing_samples = [
                thickness
                for _, thickness in samples
                if (
                    (
                        min_backfill_thickness is None
                        or thickness + 1e-9
                        >= min_backfill_thickness
                    )
                    and (
                        max_backfill_thickness is None
                        or thickness - 1e-9
                        <= max_backfill_thickness
                    )
                )
            ]
            backfill_audit[station] = {
                "sourceHatchPieces": len(backfill_parts),
                "sourceGeometryAreaCad2":
                    float(backfill_geometry.area),
                "positiveSampleCount": len(samples),
                "passingSampleCount": len(passing_samples),
                "maxThicknessM": max(
                    (thickness for _, thickness in samples),
                    default=0.0,
                ),
                "outputIntervalCount": len(filtered_intervals),
            }
        else:
            backfill_audit[station] = {
                "sourceHatchPieces": 0,
                "sourceGeometryAreaCad2": 0.0,
                "positiveSampleCount": 0,
                "passingSampleCount": 0,
                "maxThicknessM": 0.0,
                "outputIntervalCount": 0,
            }

    return blocks, assigned_piece_counts, backfill_audit


def interval_match_cost(left_interval, right_interval):
    left_a, right_a = left_interval
    left_b, right_b = right_interval
    centre_a = (left_a + right_a) / 2.0
    centre_b = (left_b + right_b) / 2.0
    centre_distance = abs(centre_a - centre_b)
    edge_gap = max(left_a, left_b) - min(right_a, right_b)
    edge_gap = max(0.0, edge_gap)
    if centre_distance > BLOCK_MATCH_MAX_CENTER_DISTANCE:
        return None
    if edge_gap > BLOCK_MATCH_MAX_EDGE_GAP:
        return None
    overlap = max(0.0, min(right_a, right_b) - max(left_a, left_b))
    width_a = right_a - left_a
    width_b = right_b - left_b
    return (
        edge_gap * 10.0
        + centre_distance
        + abs(width_a - width_b) * 0.15
        - overlap * 0.25
    )


def match_adjacent_blocks(left_blocks, right_blocks):
    candidates = []
    for left_index, left_interval in enumerate(left_blocks):
        for right_index, right_interval in enumerate(right_blocks):
            cost = interval_match_cost(left_interval, right_interval)
            if cost is not None:
                candidates.append((cost, left_index, right_index))
    candidates.sort()
    used_left = set()
    used_right = set()
    matches = []
    for cost, left_index, right_index in candidates:
        if left_index in used_left or right_index in used_right:
            continue
        used_left.add(left_index)
        used_right.add(right_index)
        matches.append((left_index, right_index, cost))
    return matches


def build_connected_uv_geometry(station_blocks):
    stations = sorted(station_blocks)
    nodes = {
        (station, index): interval
        for station in stations
        for index, interval in enumerate(station_blocks[station])
    }
    previous_links = set()
    next_links = set()
    ribbons = []
    match_records = []

    for left_station, right_station in zip(stations, stations[1:]):
        if right_station - left_station != SECTION_INTERVAL:
            continue
        left_blocks = station_blocks[left_station]
        right_blocks = station_blocks[right_station]
        for left_index, right_index, cost in match_adjacent_blocks(
            left_blocks, right_blocks
        ):
            left_interval = left_blocks[left_index]
            right_interval = right_blocks[right_index]
            ribbons.append(Polygon([
                (left_station, left_interval[0]),
                (left_station, left_interval[1]),
                (right_station, right_interval[1]),
                (right_station, right_interval[0]),
            ]))
            next_links.add((left_station, left_index))
            previous_links.add((right_station, right_index))
            match_records.append({
                "from_station": left_station,
                "from_interval": left_interval,
                "to_station": right_station,
                "to_interval": right_interval,
                "cost": cost,
            })

    for node, interval in nodes.items():
        station, _ = node
        if node not in previous_links:
            ribbons.append(box(
                station - HALF_SECTION,
                interval[0],
                station,
                interval[1],
            ))
        if node not in next_links:
            ribbons.append(box(
                station,
                interval[0],
                station + HALF_SECTION,
                interval[1],
            ))

    if not ribbons:
        return [], match_records
    geometry = unary_union(ribbons)
    return polygon_parts(geometry), match_records


def validate_uv_block_coverage(station_blocks, uv_polygons):
    """Every extracted block must survive the connection stage."""
    if not uv_polygons:
        return [
            station
            for station, intervals in station_blocks.items()
            if intervals
        ]
    geometry = unary_union(uv_polygons)
    missing = []
    for station, intervals in station_blocks.items():
        for left, right in intervals:
            centre = Point(station, (left + right) / 2.0)
            if not geometry.covers(centre):
                missing.append(station)
                break
    return sorted(set(missing))


def interpolate_spine(u, spine_data):
    station_values = sorted(spine_data)
    position = int(np.searchsorted(station_values, u))
    if position <= 0:
        spine = spine_data[station_values[0]]
        return (
            spine["spine_x"],
            spine["spine_y"],
            spine["tangent_angle"],
        )
    if position >= len(station_values):
        spine = spine_data[station_values[-1]]
        return (
            spine["spine_x"],
            spine["spine_y"],
            spine["tangent_angle"],
        )

    lower_station = station_values[position - 1]
    upper_station = station_values[position]
    lower = spine_data[lower_station]
    upper = spine_data[upper_station]
    ratio = (float(u) - lower_station) / (
        upper_station - lower_station
    )
    spine_x = lower["spine_x"] + ratio * (
        upper["spine_x"] - lower["spine_x"]
    )
    spine_y = lower["spine_y"] + ratio * (
        upper["spine_y"] - lower["spine_y"]
    )
    angle_delta = (
        upper["tangent_angle"]
        - lower["tangent_angle"]
        + math.pi
    ) % (2.0 * math.pi) - math.pi
    tangent_angle = lower["tangent_angle"] + ratio * angle_delta
    return spine_x, spine_y, tangent_angle


def uv_to_world(u, v, spine_data):
    spine_x, spine_y, tangent_angle = interpolate_spine(
        u, spine_data
    )
    cross_angle = tangent_angle + math.pi / 2.0
    return (
        spine_x + v * math.cos(cross_angle),
        spine_y + v * math.sin(cross_angle),
    )


def add_category(
    modelspace,
    category,
    uv_polygons,
    spine_data,
):
    config = CATEGORY_CONFIG[category]
    total_area = 0.0
    count = 0
    for polygon in uv_polygons:
        world_coordinates = [
            uv_to_world(float(u), float(v), spine_data)
            for u, v in polygon.exterior.coords
        ]
        if len(world_coordinates) < 3:
            continue
        modelspace.add_lwpolyline(
            world_coordinates,
            close=True,
            dxfattribs={"layer": config["layer"]},
        )
        hatch = modelspace.add_hatch(
            color=config["color"],
            dxfattribs={"layer": config["layer"]},
        )
        hatch.paths.add_polyline_path(
            world_coordinates,
            is_closed=True,
        )
        hatch.set_solid_fill(color=config["color"])
        world_polygon = Polygon(world_coordinates)
        if not world_polygon.is_valid:
            world_polygon = world_polygon.buffer(0)
        if not world_polygon.is_empty:
            total_area += float(world_polygon.area)
        count += 1
    return count, total_area


def add_uniform_station_lines(
    modelspace,
    sections,
    blocks,
    spine_data,
):
    max_abs_v = 0.0
    for category in CATEGORY_CONFIG:
        for intervals in blocks[category].values():
            for left, right in intervals:
                max_abs_v = max(max_abs_v, abs(left), abs(right))
    half_length = (
        math.ceil(
            (max_abs_v + STATION_LINE_MARGIN) / 10.0
        ) * 10.0
    )
    for section in sections:
        station = section["station"]
        spine = spine_data[station]
        cross_angle = spine["tangent_angle"] + math.pi / 2.0
        cos_angle = math.cos(cross_angle)
        sin_angle = math.sin(cross_angle)
        center_x = spine["spine_x"]
        center_y = spine["spine_y"]
        modelspace.add_line(
            (
                center_x - half_length * cos_angle,
                center_y - half_length * sin_angle,
            ),
            (
                center_x + half_length * cos_angle,
                center_y + half_length * sin_angle,
            ),
            dxfattribs={"layer": "TOPVIEW_CONTOUR"},
        )
        label = station_name(station)
        for side in (-1.0, 1.0):
            text_v = side * (
                half_length + STATION_TEXT_MARGIN
            )
            modelspace.add_text(
                label,
                height=5.0,
                dxfattribs={"layer": "TOPVIEW_CONTOUR"},
            ).set_placement((
                center_x + text_v * cos_angle,
                center_y + text_v * sin_angle,
            ))
    return half_length * 2.0


def serializable_intervals(blocks, station):
    return {
        category: [
            [round(left, 3), round(right, 3)]
            for left, right in blocks[category].get(station, [])
        ]
        for category in CATEGORY_CONFIG
    }


def station_runs(stations):
    values = sorted(set(stations))
    if not values:
        return []
    runs = []
    start = previous = values[0]
    for station in values[1:]:
        if station - previous == SECTION_INTERVAL:
            previous = station
            continue
        runs.append({
            "from": station_name(start),
            "to": station_name(previous),
            "count": (previous - start) // SECTION_INTERVAL + 1,
        })
        start = previous = station
    runs.append({
        "from": station_name(start),
        "to": station_name(previous),
        "count": (previous - start) // SECTION_INTERVAL + 1,
    })
    return runs


def validate_station_sequence(sections):
    stations = [section["station"] for section in sections]
    expected = list(range(
        stations[0],
        stations[-1] + SECTION_INTERVAL,
        SECTION_INTERVAL,
    ))
    if stations != expected:
        missing = sorted(set(expected) - set(stations))
        raise ValueError(
            "25米断面序列不完整: "
            + ", ".join(station_name(value) for value in missing[:20])
        )


def generate(
    input_dxf,
    spine_json,
    output_dxf,
    min_backfill_thickness,
    max_backfill_thickness,
):
    log("读取脊梁点匹配数据...")
    spine_data = load_spine_data(spine_json)
    log("读取断面DXF...")
    source_doc = ezdxf.readfile(input_dxf)
    modelspace = source_doc.modelspace()

    log("按平台分层算量规则匹配DMX与桩号...")
    sections = match_sections_like_platform(
        modelspace, spine_data
    )
    validate_station_sequence(sections)
    log(
        f"平台规则匹配断面: {len(sections)}，"
        "25米序列完整"
    )
    corrected_sections = [
        section for section in sections
        if section["ref_x_corrected"]
    ]
    if corrected_sections:
        log(
            "修正越出DMX范围的L1横向基准: "
            + ", ".join(
                station_name(section["station"])
                for section in corrected_sections
            )
        )
    upstream_audit, update_line_count = (
        audit_upstream_backfill_lines(modelspace, sections)
    )
    update_empty_sections = [
        station
        for station, audit in upstream_audit.items()
        if audit["updateLineCount"] == 0
    ]
    log(
        f"回淤上游线审计: {UPDATE_SECTION_LAYER} "
        f"{update_line_count}条，"
        f"未分配断面 {len(update_empty_sections)}个"
    )

    log("读取Nonem地层与回淤HATCH...")
    material_records, source_counts, source_layers = (
        load_material_polygons(modelspace)
    )
    for category, config in SOURCE_CATEGORY_CONFIG.items():
        layers = ", ".join(sorted(source_layers[category])) or "无"
        log(
            f"{config['name']}源HATCH: "
            f"{source_counts[category]}，图层: {layers}"
        )

    log("按DMX断面框提取每个断面的独立色块...")
    if (
        min_backfill_thickness is None
        and max_backfill_thickness is None
    ):
        log("回淤厚度: 不限制，输出原始有效回淤")
    else:
        log(
            "回淤厚度限制: "
            f"最小={min_backfill_thickness}, "
            f"最大={max_backfill_thickness}"
        )
    blocks, assigned_piece_counts, backfill_audit = (
        extract_section_blocks(
            sections,
            material_records,
            spine_data,
            min_backfill_thickness,
            max_backfill_thickness,
        )
    )
    for category, config in CATEGORY_CONFIG.items():
        nonempty_sections = sum(
            bool(intervals)
            for intervals in blocks[category].values()
        )
        total_blocks = sum(
            len(intervals)
            for intervals in blocks[category].values()
        )
        log(
            f"{config['name']}: {nonempty_sections}个断面，"
            f"{total_blocks}个断面块"
        )

    output_doc = ezdxf.new("R2010")
    output_modelspace = output_doc.modelspace()
    for config in CATEGORY_CONFIG.values():
        output_doc.layers.new(
            config["layer"],
            dxfattribs={"color": config["color"]},
        )
    output_doc.layers.new(
        "TOPVIEW_CONTOUR",
        dxfattribs={"color": 7},
    )

    result = {
        "success": True,
        "outputPath": output_dxf,
        "minBackfillThickness": min_backfill_thickness,
        "maxBackfillThickness": max_backfill_thickness,
        "transverseGrid": TRANSVERSE_GRID,
        "sectionInterval": SECTION_INTERVAL,
        "matchedSections": len(sections),
    }
    all_match_records = {}
    coverage_missing = {}
    for category, config in CATEGORY_CONFIG.items():
        uv_polygons, match_records = build_connected_uv_geometry(
            blocks[category]
        )
        missing_stations = validate_uv_block_coverage(
            blocks[category],
            uv_polygons,
        )
        coverage_missing[category] = missing_stations
        if missing_stations:
            raise ValueError(
                f"{config['name']}连接阶段漏传: "
                + ", ".join(
                    station_name(station)
                    for station in missing_stations[:20]
                )
            )
        count, area = add_category(
            output_modelspace,
            category,
            uv_polygons,
            spine_data,
        )
        result[f"{category.lower()}Count"] = count
        result[f"{category.lower()}Area"] = area
        section_count = sum(
            bool(intervals)
            for intervals in blocks[category].values()
        )
        result[f"{category.lower()}SectionCount"] = section_count
        result[f"{category.lower()}VisibleStations"] = section_count
        all_match_records[category] = match_records
        log(
            f"{config['name']}连接输出: {count}块，"
            f"相邻连接 {len(match_records)}条，"
            f"面积 {area:,.0f} m2"
        )

    transmission_loss = [
        station
        for station, audit in backfill_audit.items()
        if (
            audit["passingSampleCount"] > 0
            and not blocks["BACKFILL"][station]
        )
    ]
    if transmission_loss:
        raise ValueError(
            "回淤厚度筛选阶段漏传: "
            + ", ".join(
                station_name(station)
                for station in transmission_loss[:20]
            )
        )
    source_empty = [
        station
        for station, audit in backfill_audit.items()
        if audit["sourceHatchPieces"] == 0
    ]
    threshold_filtered = [
        station
        for station, audit in backfill_audit.items()
        if (
            audit["sourceHatchPieces"] > 0
            and audit["passingSampleCount"] == 0
        )
    ]
    upstream_hatch_loss = [
        station
        for station, audit in upstream_audit.items()
        if (
            audit["positiveGapSampleCount"] > 0
            and backfill_audit[station]["sourceHatchPieces"] == 0
        )
    ]
    upstream_zero = [
        station
        for station, audit in upstream_audit.items()
        if audit["positiveGapSampleCount"] == 0
    ]
    log(
        "回淤全段传导审计: "
        f"源HATCH为空 {len(source_empty)}个断面，"
        f"厚度筛除 {len(threshold_filtered)}个断面，"
        f"上游线到HATCH漏传 {len(upstream_hatch_loss)}个断面，"
        "俯视图中间漏传 0个断面"
    )
    result["sourceHatchEmptySectionCount"] = len(source_empty)
    result["thresholdFilteredSectionCount"] = len(
        threshold_filtered
    )
    result["upstreamLineToHatchLossCount"] = len(
        upstream_hatch_loss
    )
    result["transmissionLossCount"] = len(transmission_loss)

    line_length = add_uniform_station_lines(
        output_modelspace,
        sections,
        blocks,
        spine_data,
    )
    result["stationLineCount"] = len(sections)
    result["stationTextCount"] = len(sections) * 2
    result["stationLineLength"] = line_length
    log(
        f"统一桩号线: {len(sections)}条，"
        f"每条长 {line_length:.1f}m，双侧标注"
    )

    output_parent = os.path.dirname(os.path.abspath(output_dxf))
    os.makedirs(output_parent, exist_ok=True)
    output_doc.saveas(output_dxf)

    debug_path = str(Path(output_dxf).with_suffix(
        ".sections.json"
    ))
    debug_data = {
        "inputDxf": input_dxf,
        "spineJson": spine_json,
        "outputDxf": output_dxf,
        "rules": {
            "sectionAssignment": (
                "Qt EngineCad::loadSectionData + "
                "StationMatcher::matchSectionToStation"
            ),
            "sectionInterval": SECTION_INTERVAL,
            "transverseGrid": TRANSVERSE_GRID,
            "blockMatchMaxCenterDistance":
                BLOCK_MATCH_MAX_CENTER_DISTANCE,
            "blockMatchMaxEdgeGap": BLOCK_MATCH_MAX_EDGE_GAP,
            "minBackfillThickness": min_backfill_thickness,
            "maxBackfillThickness": max_backfill_thickness,
        },
        "correctedSectionReferences": [
            {
                "station": station_name(section["station"]),
                "sourceL1X": section["source_l1_x"],
                "dmxCenterX": section["center_x"],
                "correctedRefX": section["ref_x"],
            }
            for section in corrected_sections
        ],
        "backfillAuditSummary": {
            "sourceHatchEmptySectionCount": len(source_empty),
            "sourceHatchEmptyRuns": station_runs(source_empty),
            "sourceUpdateLineEmptySectionCount":
                len(update_empty_sections),
            "sourceUpdateLineEmptyRuns":
                station_runs(update_empty_sections),
            "sourcePositiveGapEmptySectionCount":
                len(upstream_zero),
            "sourcePositiveGapEmptyRuns":
                station_runs(upstream_zero),
            "upstreamLineToHatchLossStations": [
                station_name(station)
                for station in upstream_hatch_loss
            ],
            "thresholdFilteredSectionCount":
                len(threshold_filtered),
            "thresholdFilteredRuns":
                station_runs(threshold_filtered),
            "transmissionLossStations": [
                station_name(station)
                for station in transmission_loss
            ],
            "connectionCoverageMissing": {
                category: [
                    station_name(station)
                    for station in stations
                ]
                for category, stations in coverage_missing.items()
            },
        },
        "backfillAudit": {
            station_name(station): {
                "sourceHatchPieces":
                    audit["sourceHatchPieces"],
                "sourceGeometryAreaCad2": round(
                    audit["sourceGeometryAreaCad2"], 6
                ),
                "positiveSampleCount":
                    audit["positiveSampleCount"],
                "passingSampleCount":
                    audit["passingSampleCount"],
                "maxThicknessM": round(
                    audit["maxThicknessM"], 6
                ),
                "outputIntervalCount":
                    audit["outputIntervalCount"],
                "updateLineCount":
                    upstream_audit[station]["updateLineCount"],
                "signedGapSampleCount":
                    upstream_audit[station]["sampleCount"],
                "positiveGapSampleCount":
                    upstream_audit[station][
                        "positiveGapSampleCount"
                    ],
                "maxPositiveThicknessM": round(
                    upstream_audit[station][
                        "maxPositiveThicknessM"
                    ],
                    6,
                ),
                "minSignedThicknessM": round(
                    upstream_audit[station][
                        "minSignedThicknessM"
                    ],
                    6,
                ),
                "maxSignedThicknessM": round(
                    upstream_audit[station][
                        "maxSignedThicknessM"
                    ],
                    6,
                ),
            }
            for station, audit in sorted(backfill_audit.items())
        },
        "checkStations": {
            station_name(station): serializable_intervals(
                blocks, station
            )
            for station in (
                list(range(67425, 67951, 25))
                + list(range(71375, 71526, 25))
            )
            if station in spine_data
        },
        "checkConnections": {
            category: [
                {
                    "from": station_name(record["from_station"]),
                    "fromInterval": [
                        round(record["from_interval"][0], 3),
                        round(record["from_interval"][1], 3),
                    ],
                    "to": station_name(record["to_station"]),
                    "toInterval": [
                        round(record["to_interval"][0], 3),
                        round(record["to_interval"][1], 3),
                    ],
                }
                for record in all_match_records[category]
                if (
                    record["from_station"] in (
                        set(range(67425, 67951, 25))
                        | set(range(71375, 71526, 25))
                    )
                    or record["to_station"] in (
                        set(range(67425, 67951, 25))
                        | set(range(71375, 71526, 25))
                    )
                )
            ]
            for category in CATEGORY_CONFIG
        },
        "result": result,
    }
    with open(debug_path, "w", encoding="utf-8") as stream:
        json.dump(
            debug_data,
            stream,
            ensure_ascii=False,
            indent=2,
        )
    result["debugPath"] = debug_path
    return result


def main():
    parser = argparse.ArgumentParser(
        description="v4.1逐断面色块连接俯视图"
    )
    parser.add_argument("input_dxf")
    parser.add_argument("spine_json")
    parser.add_argument("--output", required=True)
    parser.add_argument(
        "--min-backfill-thickness",
        type=float,
        default=None,
        help="可选：仅显示不小于该厚度的回淤；默认不限制",
    )
    parser.add_argument(
        "--max-backfill-thickness",
        type=float,
        default=None,
        help="可选：仅显示不大于该厚度的回淤；默认不限制",
    )
    args = parser.parse_args()

    try:
        if (
            args.min_backfill_thickness is not None
            and args.min_backfill_thickness < 0
        ):
            raise ValueError("回淤最小厚度不能小于0")
        if (
            args.max_backfill_thickness is not None
            and args.max_backfill_thickness < 0
        ):
            raise ValueError("回淤最大厚度不能小于0")
        if (
            args.min_backfill_thickness is not None
            and args.max_backfill_thickness is not None
            and args.min_backfill_thickness
            > args.max_backfill_thickness
        ):
            raise ValueError("回淤最小厚度不能大于最大厚度")
        result = generate(
            args.input_dxf,
            args.spine_json,
            args.output,
            args.min_backfill_thickness,
            args.max_backfill_thickness,
        )
    except Exception as error:
        result = {
            "success": False,
            "error": str(error),
        }
        print(f"[ERROR] {error}", file=sys.stderr, flush=True)

    print(
        "__RESULT__:"
        + json.dumps(result, ensure_ascii=False),
        flush=True,
    )
    return 0 if result.get("success") else 1


if __name__ == "__main__":
    raise SystemExit(main())
