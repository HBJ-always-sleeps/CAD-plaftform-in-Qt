# HydraulicCADPlatform

航道断面算量自动化平台 — 基于 C++/Qt6 的航道工程断面计算工具

## 功能概述

- **断面包络线生成** — 自动生成航道横断面的上下包络线
- **分层算量** — 按地质分层（淤泥、砂等）计算挖方量，支持设计线与超挖线
- **回填计算** — 回填面积与方量计算
- **批量粘贴** — 按桩号自动匹配的批量断面粘贴
- **快速填充** — 带面积计算的快速 HATCH 填充
- **DXF 解析** — 直接读取中望CAD/AutoCAD导出的 DXF 文件（GBK 编码）
- **Excel 导出** — 计算结果导出为多 Sheet 的 XLSX 文件

## 技术架构

```
MainWindow (Qt6 GUI)
    └─> TaskWorker (QThread)
         └─> EngineCad (C++ core)
              ├─> DXFWrapper (C++ DXF parser)
              ├─> C++ geometry operations (Line2D, Polygon2D, Box2D)
              └─> QProcess → scripts/autosection_compute.py
                   ├─ Shapely polygon boolean operations
                   ├─ ezdxf DXF output with HATCH fills
                   └─ pandas/openpyxl XLSX output
```

C++ 负责 DXF 解析、几何提取、UI 和流程编排；Python (Shapely) 负责精确的多边形布尔运算和输出生成。

## 构建

### 依赖

- Qt 6.8.3 (MSVC 2022 x64)
- Visual Studio Build Tools (MSVC)
- CMake 3.16+
- Python 3.8+（用于计算脚本）

### Python 包

```powershell
pip install shapely ezdxf pandas openpyxl numpy
```

### 编译

```powershell
cd build
cmake -G "Visual Studio 18" -A x64 -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 ..
msbuild HydraulicCADPlatform.vcxproj /p:Configuration=Debug
```

编译产物输出到 `bin/Debug/` 或 `bin/Release/`。

### 构建目标

| 目标 | 说明 |
|------|------|
| `HydraulicCADPlatform` | 主程序 GUI |
| `TestAutosection` | 分层算量+回填集成测试 |
| `TestDxfLayers` | DXF 图层解析验证 |
| `TestDxfSimple` | DXF 实体计数基准 |
| `TestFileRead` | 文件读取速度基准 |

## 使用

1. 启动程序后加载 DXF 文件
2. 选择断面、设置参数（目标高程、计算模式、地层等）
3. 执行计算，自动生成 DXF 图纸和 Excel 报表

## 项目结构

```
├── src/            # C++ 源码（MainWindow, EngineCad, DXFWrapper 等）
├── include/        # 头文件（Config, Geometry, utils/）
├── scripts/        # Python 计算脚本
├── original_scripts/ # 原始 Python/PyQt6 版本（参考）
├── thirdparty/     # 第三方库
└── build/          # 构建目录
```

## License

MIT
