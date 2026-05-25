# 第三方库安装指南

本文档提供dxflib和QtXlsx的详细安装步骤，实现完整的DXF读写和Excel导出功能。

---

## 一、dxflib安装

### 1.1 下载dxflib

**官方地址**: https://sourceforge.net/projects/dxflib/

**版本**: dxflib 3.x

**文件大小**: ~500KB

### 1.2 安装步骤

#### 方式A：直接编译源文件（推荐）

```batch
# 1. 下载并解压
# 将dxflib解压到 D:\Qt自动算量平台\dxflib\

# 2. 编辑 HydraulicCADPlatform.pro，添加：
DEFINES += USE_DXFLIB

SOURCES += $$PWD/dxflib/src/dl_dxf.cpp \
           $$PWD/dxflib/src/dl_writer_ascii.cpp

HEADERS += $$PWD/dxflib/src/dl_dxf.h \
           $$PWD/dxflib/src/dl_writer.h \
           $$PWD/dxflib/src/dl_writer_ascii.h \
           $$PWD/dxflib/src/dl_global.h \
           $$PWD/dxflib/src/dl_entities.h \
           $$PWD/dxflib/src/dl_attributes.h
```

#### 方式B：预编译库

```batch
# 1. 编译dxflib
cd dxflib
qmake dxflib.pro
nmake  # 或 mingw32-make

# 2. 编辑 HydraulicCADPlatform.pro：
DEFINES += USE_DXFLIB
INCLUDEPATH += $$PWD/dxflib/src
LIBS += -L$$PWD/dxflib -ldxflib
```

### 1.3 dxflib核心API

```cpp
#include "dl_dxf.h"
#include "dl_creationadapter.h"
#include "dl_writer_ascii.h"

// 读取DXF
DL_Dxf dxf;
DXFCreationAdapter adapter;  // 需要继承DL_CreationAdapter
dxf.in(fileHandle, &adapter);

// 写入DXF
DL_Dxf dxf;
DL_WriterA* dw = dxf.out(fileHandle, DL_VERSION_R12);

// 写实体
DL_LineData line(x1, y1, z1, x2, y2, z2, layer);
dw->writeLine(line);

DL_PolylineData poly(numVertices, flags, 0, 0);
dw->writePolyline(poly);
dw->writeVertex(vertex);
dw->writePolylineEnd();

dw->writeHatch(hatchData);
dw->writeHatchLoop(loopData);
dw->writeHatchEdge(edgeData);
dw->writeHatchEnd();
```

---

## 二、QtXlsx安装

### 2.1 下载QtXlsx

**GitHub**: https://github.com/dbarabanov/QtXlsxWriter

**版本**: QtXlsx 1.x

### 2.2 安装步骤

#### 方式A：作为子项目（推荐）

```batch
# 1. 克隆到项目目录
git clone https://github.com/dbarabanov/QtXlsxWriter.git QtXlsx

# 2. 编辑 HydraulicCADPlatform.pro：
DEFINES += USE_QTXLSX
include($$PWD/QtXlsx/src/xlsx/qtxlsx.pri)
```

#### 方式B：Qt模块方式

```batch
# 1. 安装QtXlsx模块
cd QtXlsx
qmake
nmake install

# 2. 编辑 HydraulicCADPlatform.pro：
DEFINES += USE_QTXLSX
QT += xlsx  # 如果已安装为Qt模块
```

### 2.3 QtXlsx核心API

```cpp
#include "xlsxdocument.h"
#include "xlsxworkbook.h"
#include "xlsxworksheet.h"
#include "xlsxformat.h"

using namespace QXlsx;

// 创建文档
Document xlsx;

// 添加Sheet
xlsx.addSheet("SheetName");

// 写入单元格
xlsx.write(row, col, value);

// 设置格式
Format fmt;
fmt.setFontBold(true);
fmt.setHorizontalAlignment(Format::AlignHCenter);
fmt.setBorderStyle(Format::BorderThin);
xlsx.write(row, col, value, fmt);

// 保存
xlsx.saveAs("output.xlsx");
```

---

## 三、验证安装

### 3.1 检查宏定义

编辑 `HydraulicCADPlatform.pro`，确保：

```qmake
DEFINES += USE_DXFLIB
DEFINES += USE_QTXLSX
```

### 3.2 检查代码编译

编译后检查输出：
- 如果定义了 `USE_DXFLIB`，DXFWrapper.cpp中的实际代码会被编译
- 如果没有定义，mock实现会被编译（返回空结果）

### 3.3 运行测试

```batch
cd D:\Qt自动算量平台
compile_test.bat

# 或
qmake
nmake
bin\HydraulicCADPlatform.exe
```

---

## 四、Boost.Geometry安装（可选）

用于精确几何运算（多边形交集、差集等）。

### 4.1 下载Boost

**官网**: https://www.boost.org/

**版本**: Boost 1.84+

### 4.2 安装

```batch
# 下载并解压到 C:\boost_1_84_0\

# 编辑 HydraulicCADPlatform.pro：
INCLUDEPATH += C:/boost_1_84_0

# 使用示例：
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/algorithms/intersection.hpp>

namespace bg = boost::geometry;

typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
typedef bg::model::polygon<point_t> polygon_t;

// 多边形交集
polygon_t poly1, poly2, result;
bg::intersection(poly1, poly2, result);
```

---

## 五、完整集成示例

### HydraulicCADPlatform.pro 完整配置

```qmake
QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = HydraulicCADPlatform
TEMPLATE = app

# ==================== 编译定义 ====================
DEFINES += QT_DEPRECATED_WARNINGS

# 启用dxflib
DEFINES += USE_DXFLIB

# 启用QtXlsx
DEFINES += USE_QTXLSX

# ==================== dxflib ====================
SOURCES += $$PWD/dxflib/src/dl_dxf.cpp \
           $$PWD/dxflib/src/dl_writer_ascii.cpp

HEADERS += $$PWD/dxflib/src/dl_dxf.h \
           $$PWD/dxflib/src/dl_writer.h \
           $$PWD/dxflib/src/dl_writer_ascii.h \
           $$PWD/dxflib/src/dl_global.h \
           $$PWD/dxflib/src/dl_entities.h \
           $$PWD/dxflib/src/dl_attributes.h

# ==================== QtXlsx ====================
include($$PWD/QtXlsx/src/xlsx/qtxlsx.pri)

# ==================== Boost（可选） ====================
# INCLUDEPATH += C:/boost_1_84_0

# ==================== 源文件 ====================
SOURCES += \
    main.cpp \
    MainWindow.cpp \
    EngineCad.cpp \
    DXFWrapper.cpp \
    ExcelExporter.cpp \
    FileRowWidget.cpp \
    ParamInputWidget.cpp \
    ParamSelectWidget.cpp \
    ParamCheckboxWidget.cpp \
    TaskWorker.cpp

# ==================== 头文件 ====================
HEADERS += \
    MainWindow.h \
    EngineCad.h \
    Config.h \
    Geometry.h \
    LineUtils.h \
    EnvelopeGenerator.h \
    RulerDetector.h \
    StationMatcher.h \
    VirtualBoxBuilder.h \
    LayerExtractor.h \
    HatchProcessor.h \
    OutputHelper.h \
    EntityHelper.h \
    DXFWrapper.h \
    ExcelExporter.h \
    FileRowWidget.h \
    ParamInputWidget.h \
    ParamSelectWidget.h \
    ParamCheckboxWidget.h \
    TaskWorker.h

# ==================== 输出目录 ====================
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui
```

---

## 六、常见问题

### Q1: 编译错误 "dl_dxf.h not found"

**解决**: 确保 `USE_DXFLIB` 宏已定义，且dxflib源文件已添加到项目。

### Q2: 编译错误 "xlsxdocument.h not found"

**解决**: 确保 `USE_QTXLSX` 宏已定义，且QtXlsx已通过 `include()` 添加。

### Q3: 运行时DXF功能无效

**解决**: 检查 `USE_DXFLIB` 是否定义。如果没有，代码使用mock实现。

### Q4: QtXlsx编译错误

**解决**: QtXlsx需要Qt 5.x或6.x。确保Qt版本兼容。

---

*安装指南版本: v1.0*
*更新时间: 2026-04-27*