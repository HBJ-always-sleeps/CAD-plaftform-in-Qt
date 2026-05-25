# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HydraulicCADPlatform v3.7.0 (航道断面算量自动化平台) - a C++17/Qt6 application for hydraulic channel cross-section quantity calculations. Migrated from Python/PyQt6 (`original_scripts/`), with Python retained for mathematically precise polygon operations via Shapely. No third-party C++ dependencies required by default — uses a custom DXF parser (`DXFWrapper`) and CSV fallback for Excel export.

## Build Commands

### CMake + MSVC (primary build method)

```powershell
# Configure (first time or after CMakeLists.txt changes)
cd D:\QtCADPlatform\build
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" && cmake -G "Visual Studio 18" -A x64 -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 ..'

# Build targets (VsDevCmd.bat must be in PATH for msbuild)
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" && msbuild HydraulicCADPlatform.vcxproj /p:Configuration=Debug /v:minimal'
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" && msbuild TestAutosection.vcxproj /p:Configuration=Debug /v:minimal'
```

Executables output to `bin/Debug/` or `bin/Release/`.

### Build targets

| Target | Description |
|--------|-------------|
| `HydraulicCADPlatform` | Main GUI application |
| `TestAutosection` | Integration test for autosection+backfill tasks |
| `TestDxfLayers` | DXF layer parsing verification |
| `TestDxfSimple` | DXF entity counting benchmark |
| `TestFileRead` | File read speed benchmark |

### Optional third-party (not required)

CMake options `USE_DXFLIB` and `USE_QTXLSX` look in `thirdparty/` for dxflib and QtXlsxWriter, but neither directory exists by default. The project compiles without them:
- Without dxflib: `DXFWrapper.cpp` provides full DXF read/write as a custom text parser
- Without QtXlsx: `ExcelExporter` falls back to CSV output

### Python dependencies

The Python scripts in `scripts/` require:
```powershell
pip install shapely ezdxf pandas openpyxl numpy
```

## Architecture

### Build state

Project is ~80% complete per `REFACTOR_PROGRESS.md`. Main gaps: dxflib integration for advanced entity operations, complete geometric algorithms (C++ polygon boolean ops are simplified — use Python for precision), QtXlsx for native Excel output.

### Hybrid C++/Python design

C++ handles DXF parsing, geometry extraction, UI, and orchestration. Python handles mathematically precise polygon boolean operations (Shapely) and output generation (ezdxf for DXF, pandas+openpyxl for Excel).

```
MainWindow (Qt6 GUI)
    └─> TaskWorker (QThread)
         └─> EngineCad (C++ core)
              ├─> DXFWrapper (C++ DXF parser)
              ├─> C++ geometry operations (Line2D, Polygon2D, Box2D)
              └─> QProcess → scripts/autosection_compute.py
                   ├─ JSON input (section data, strata layers, parameters)
                   ├─ Shapely polygon operations
                   ├─ ezdxf DXF output with HATCH fills
                   └─ pandas/openpyxl XLSX output (multi-sheet)
```

### C++ → Python data exchange

`EngineCad::runPythonComputation()` exports section data as JSON, calls Python via QProcess (5-minute timeout), then cleans up the JSON file. Python reads JSON, computes, writes DXF+XLSX, and prints a result line to stdout:

```
__RESULT__:{"totalArea": 1234.5, "backfillArea": 567.8}
```

C++ parses this line to get computed areas. If the line is missing or malformed, the C++ side treats the computation as failed.

JSON structure:
```json
{
    "task_type": "autosection" | "autosection_backfill",
    "input_dxf": "path/to/source.dxf",
    "output_dxf": "path/to/output.dxf",
    "output_xlsx": "path/to/output.xlsx",
    "target_elevation": -4.0,
    "calc_mode": "above" | "below",
    "strata_layers": ["1级淤泥", "3级砂", ...],
    "sections": [{"station": "K67+400", "dmx_points": [[x,y],...], "aux_lines": [...]}],
    "excav_lines": [{"points": [[x,y],...]}]
}
```

### Core engine tasks (EngineCad)

Six tasks in `src/EngineCad.cpp`:
- **runAutoline** - Cross-section envelope generation (upper/lower)
- **runAutopaste** - Batch paste with station matching v2
- **runAutohatch** - Quick hatch fill with area calculation
- **runAutosection** - Stratified volume calculation (design vs over-excavation)
- **runBackfill** - Backfill area calculation
- **runAutosectionBackfill** - Combined stratified + backfill

### Custom geometry types (include/Geometry.h)

Lightweight types replacing Shapely for C++ side: `Point2D`, `Line2D` (polyline), `Polygon2D` (exterior + interiors, Shoelace area), `Box2D` (bounding box), `MultiPolygon2D`. `GeometryUtils` provides simplified polygon operations:
- `polygonIntersection()` — Sutherland-Hodgman clipping
- `polygonDifference()` — split-based difference
- `polygonize()` — line segments to polygons via connectivity
- `computeConvexHull()` — Graham Scan

The Python side uses actual Shapely for precise boolean operations where the C++ simplifications are insufficient.

### ExcelExporter (src/ExcelExporter.h)

Static export methods for all task results. When `USE_QTXLSX` is enabled, writes multi-sheet XLSX via QtXlsxWriter; otherwise falls back to CSV. The Python side (`autosection_compute.py`) also generates XLSX via pandas+openpyxl — the C++ ExcelExporter is mainly for tasks that don't call Python.

### Utility headers (include/utils/)

Header-only utilities: `EnvelopeGenerator.h` (upper/lower envelope via min/max Y sampling), `LineUtils.h` (linear interpolation, segment intersection, line merging), `StationMatcher.h` (regex-based `K67+400` parsing and nearest-neighbor matching), `RulerDetector.h` (per-section scale detection via INSERT entity proximity + linear regression), `VirtualBoxBuilder.h` (Y-coordinate clustering for over-excavation sections), `LayerExtractor.h` (entity filtering, strata layer detection via `^\d+级` regex), `HatchProcessor.h` (HATCH to Polygon2D conversion, area labels).

## DXF Encoding

DXF files from Chinese CAD software (中望CAD, AutoCAD CN) use GBK/ANSI_936 encoding. All DXF string I/O must use `QStringDecoder("GB18030")` / `QStringEncoder("GB18030")`, never `QString::fromLocal8Bit()`.

```cpp
static QString fromGBK(const QByteArray &bytes) {
    static QStringDecoder decoder("GB18030");
    return decoder.decode(bytes);
}
```

DXF format: AC1032, `$DWGCODEPAGE` = `ANSI_936`.

## Key Patterns

### Layer name matching

Chinese layer names require Unicode codepoint comparison due to encoding variability:
```cpp
for (QChar c : layerName) {
    if (c.unicode() == 0x65AD) hasDuan = true;  // 断
    if (c.unicode() == 0x9762) hasMian = true;  // 面
}
```

### Station number format

`K69+400` → integer value 69400. `Config::parseSourceStation()` / `Config::formatStation()` handle conversion.

### Strata layer detection

Layers matching `^\d+级` regex (e.g., "1级淤泥", "3级砂"). Color mapping in `Config::STRATA_COLORS`.

### Ruler detection

Per-section detection: find nearest ruler INSERT by X proximity, pick best by Y overlap, linear regression on elevation points to get `y = a*elev + b`. Fallback: `y = 5*elev - 27`.

## Test Data

Test DXF files are at `D:\断面算量平台\测试文件\`. The main test file is `202511.dxf` with 245 DMX cross-sections. `TestAutosection` runs two scenarios:
1. Full volume + backfill (elevation=0, below mode)
2. Stratified above elevation -4m (above mode)

## Common Issues

### Encoding corruption

If layer names show `U+FFFD`, encoding is wrong. Use GB18030 decoder.

### Python script not found

`runPythonComputation()` looks for `scripts/autosection_compute.py` relative to the source tree. If not found, falls back to hardcoded `D:/QtCADPlatform/scripts/autosection_compute.py`.

### cmake not in PATH

Use `-DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64`.

### DMX layer empty

DMX entities are on layer "DMX" (exact match). Verify with PowerShell:
```powershell
$file = "path.dxf"; $bytes = [IO.File]::ReadAllBytes($file)
$content = [Text.Encoding]::GetEncoding("GBK").GetString($bytes)
($content -split "\r?\n") | where { $_ -eq "DMX" }
```
