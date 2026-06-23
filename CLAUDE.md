# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HydraulicCADPlatform v3.7.0 (航道断面算量自动化平台) - a C++17/Qt6 application for hydraulic channel cross-section quantity calculations. Migrated from Python/PyQt6 (`original_scripts/`), with Python retained for mathematically precise polygon operations via Shapely. No third-party C++ dependencies required by default — uses a custom DXF parser (`DXFWrapper`) and CSV fallback for Excel export.

The `refactor/` directory contains v3.9.0 — a cleaned-up version with ~60% less code (6112→2407 lines). Key changes: 5 dead headers deleted, dead stubs removed from EngineCad, static string constants for log levels, Bounds2D removed (Box2D only), autopaste v2 fully implemented. Both versions share the same Python scripts and test data.

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

### Running the built executable

Qt DLLs must be on PATH. Either use `windeployqt` or set PATH manually:
```powershell
$env:PATH = "D:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
.\bin\Debug\HydraulicCADPlatform.exe
.\bin\Debug\TestAutosection.exe
```

### Refactored codebase (refactor/)

`refactor/` is a parallel copy of the project (v3.8.0) with cleaner code. It has its own build directory and convenience scripts:
```powershell
# Build and run the integration test
powershell -NoProfile -File "D:/QtCADPlatform/refactor/build_test.ps1"
powershell -NoProfile -Command '$env:PATH = "D:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"; & "D:\QtCADPlatform\refactor\bin\Debug\TestAutosection.exe"'

# Build the main GUI
powershell -NoProfile -File "D:/QtCADPlatform/refactor/build_all.ps1"
```

### qmake (alternative)

```powershell
set QTDIR=D:\Qt\6.8.3\msvc2022_64
%QTDIR%\bin\qmake.exe HydraulicCADPlatform.pro -spec win32-msvc
nmake
```

### Build targets

| Target | Description |
|--------|-------------|
| `HydraulicCADPlatform` | Main GUI application |
| `TestAutosection` | Integration test for autosection+backfill tasks |
| `TestV4` | V4 section-specific autosection test |
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
    "excav_lines": [{"points": [[x,y],...]}],
    "overexc_lines": [{"points": [[x,y],...]}]
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

Header-only utilities: `EnvelopeGenerator.h` (upper/lower envelope via min/max Y sampling), `LineUtils.h` (linear interpolation, segment intersection), `StationMatcher.h` (regex-based `K67+400` parsing and nearest-neighbor matching), `VirtualBoxBuilder.h` (Y-coordinate clustering for over-excavation sections).

In the refactored codebase, 5 headers were deleted (EntityHelper, HatchProcessor, RulerDetector, OutputHelper, LayerExtractor) — their used functions were inlined into EngineCad.cpp. Ruler detection is now a static function `detectRulerScaleFromDXF()` in EngineCad.cpp. Strata layer detection is `detectStrataLayers()`. Output path helpers are `getOutputPathStatic()` and `buildBackfillOutputNameStatic()`.

### Reference implementations

`original_scripts/` contains the original Python/PyQt6 code (`engine_cad_v3.py`, `platform_ui_v3.py`). When porting or debugging, compare against these. `scripts/compare_summary.md` documents output parity between the Python original and C++ hybrid.

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

`K69+400` → integer value 69400. `Config::parseStation()` / `Config::formatStation()` handle conversion.

### Strata layer detection

Layers matching `^\d+级` regex (e.g., "1级淤泥", "3级砂"). Color mapping in `Config::STRATA_COLORS`.

### DXF layer structure for autosection

Three boundary layers define the excavation area:
- **开挖线** (U+5F00 U+6316 U+7EBF) — Inner excavation boundary (1420 entities)
- **超挖线** (U+8D85 U+6316 U+7EBF) — Outer over-excavation boundary (1564 entities)
- **地质分层** (U+5730 U+8D28 U+5206 U+5C42) — Geological strata boundaries

The 超挖线 extends further than 开挖线. Section extension must cover both layers to ensure complete hatch fills. The `overexc_lines` JSON field passes 超挖线 data from C++ to Python.

### Log level constants (refactored codebase)

EngineCad.cpp uses static string constants for log levels to avoid重复 `QStringLiteral` allocations:
```cpp
static const QString kInfo    = QStringLiteral("info");
static const QString kError   = QStringLiteral("error");
static const QString kWarn    = QStringLiteral("warning");
static const QString kSuccess = QStringLiteral("success");
```
These are used both as log level parameters and as JSON result keys (`result[kSuccess] = true`).

### Ruler detection

Per-section detection: find nearest ruler INSERT by X proximity, pick best by Y overlap, linear regression on elevation points to get `y = a*elev + b`. Fallback: `y = 5*elev - 27`.

## Test Data

Test DXF files are at `D:\断面算量平台\测试文件\`. The main test file is `202511.dxf` with 245 DMX cross-sections. `TestAutosection` runs two scenarios:
1. Full volume + backfill (elevation=0, below mode)
2. Stratified above elevation -4m (above mode)

### Build scripts

Convenience PowerShell scripts for building:
- `build_v4.ps1` — Build TestV4 target (V4 section-specific test)
- `build_main.ps1` — Build main GUI application
- `build_test.ps1` — Build TestAutosection target

### Verification scripts

`scripts/verify_boundary.py` — Verifies that design/over-excavation boundary strictly follows the excavation line:
```powershell
python scripts/verify_boundary.py "path/to/source.dxf" --section-layer V4 --sample 10
```

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

### qDebug() output not visible on Windows

On Windows Debug builds, `qDebug()` uses `OutputDebugString`, not stdout/stderr. To see output in a test executable, install a message handler:
```cpp
#include <cstdio>
static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg) {
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}
// In main():
qInstallMessageHandler(messageHandler);
```

### Layer name matching: exact vs fuzzy

`matchLinesByLayer()` (original only — removed in refactor) does fuzzy matching — any single character from the set matches. For critical entity extraction (DMX lines, update lines), prefer exact matching via `getEntityList()` or direct `line.layerName == layerName` comparison. Fuzzy matching with Chinese characters like 断/面 will over-match layers containing those characters individually.

### Chinese path issues in build scripts

PowerShell build scripts set `TEMP=C:\temp` to avoid Chinese characters in temp paths breaking MSVC tools. Ensure `C:\temp` exists or the scripts create it automatically.
