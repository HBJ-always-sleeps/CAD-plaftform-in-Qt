# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HydraulicCADPlatform (航道断面算量自动化平台) - a C++/Qt6 application for hydraulic channel cross-section quantity calculations. The project is a migration from a Python/PyQt6 original (`original_scripts/`) to C++/Qt6, with Python retained for mathematically precise polygon operations via Shapely.

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

### Python dependencies

The Python scripts in `scripts/` require:
```powershell
pip install shapely ezdxf pandas openpyxl numpy
```

## Architecture

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

`EngineCad::runPythonComputation()` exports section data as JSON, calls Python via QProcess (5-minute timeout), then cleans up the JSON file. Python reads JSON, computes, and writes DXF+XLSX directly.

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

Lightweight types replacing Shapely for C++ side: `Point2D`, `Line2D` (polyline), `Polygon2D` (exterior + interiors, Shoelace area), `Box2D` (bounding box). The Python side uses actual Shapely for precise boolean operations.

### Utility headers (include/utils/)

Header-only utilities: `EnvelopeGenerator.h` (upper/lower envelope), `LineUtils.h` (interpolation, intersection), `StationMatcher.h` (K67+400 parsing), `RulerDetector.h` (scale detection via linear regression), `VirtualBoxBuilder.h` (Y-clustering), `LayerExtractor.h`, `HatchProcessor.h`.

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
