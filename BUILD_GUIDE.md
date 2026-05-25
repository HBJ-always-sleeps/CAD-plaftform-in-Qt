# Qt自动算量平台 - 完整构建指南

## 目录

1. [系统要求](#系统要求)
2. [快速开始](#快速开始)
3. [详细构建步骤](#详细构建步骤)
4. [第三方库集成](#第三方库集成)
5. [编译测试脚本](#编译测试脚本)
6. [常见问题](#常见问题)

---

## 系统要求

### 必需
- **Qt 6.x** 开发环境（Qt Creator 或命令行）
- **C++17** 编译器支持
- Windows/Linux/macOS

### 可选（用于完整功能）
- **dxflib** - DXF文件读写
- **QtXlsx** - Excel导出
- **Boost.Geometry** - 精确几何运算

---

## 快速开始

### Windows (Qt Creator)

```
1. 打开 Qt Creator
2. 文件 > 打开文件或项目 > 选择 HydraulicCADPlatform.pro
3. 配置项目（选择Qt 6.x版本）
4. 构建 > 构建项目
5. 运行
```

### Windows (命令行)

```batch
cd D:\Qt自动算量平台

# 设置Qt环境变量（如果需要）
set PATH=C:\Qt\6.x\msvc2019_64\bin;%PATH%

# 运行编译测试脚本
compile_test.bat
```

### Linux/macOS

```bash
cd /path/to/Qt自动算量平台

# 运行编译测试脚本
chmod +x compile_test.sh
./compile_test.sh
```

---

## 详细构建步骤

### 步骤1：安装Qt

#### Windows
1. 从 https://www.qt.io/download 下载Qt Online Installer
2. 安装时选择：
   - Qt 6.x for Desktop (MSVC/MinGW)
   - Qt Creator
3. 设置环境变量：
   ```batch
   set QTDIR=C:\Qt\6.x\msvc2019_64
   set PATH=%QTDIR%\bin;%PATH%
   ```

#### Linux
```bash
# Ubuntu/Debian
sudo apt install qt6-base-dev qt6-tools-dev

# Fedora
sudo dnf install qt6-qtbase-devel qt6-qttools-devel
```

### 步骤2：检查项目文件

```batch
# 检查头文件数量
dir *.h /b | find /c /v ""

# 检查源文件数量  
dir *.cpp /b | find /c /v ""
```

预期输出：
- 头文件：约20个
- 源文件：约10个

### 步骤3：生成Makefile

```batch
qmake HydraulicCADPlatform.pro -o build\Makefile

# 或指定编译器
qmake -spec win32-msvc HydraulicCADPlatform.pro
qmake -spec win32-g++  HydraulicCADPlatform.pro
```

### 步骤4：编译

```batch
# MSVC
nmake

# MinGW
mingw32-make -j4

# Linux/macOS
make -j4
```

### 步骤5：运行

```batch
# Windows
bin\HydraulicCADPlatform.exe

# Linux/macOS
./bin/HydraulicCADPlatform
```

---

## 第三方库集成

### 1. dxflib - DXF文件读写

#### 下载
- https://sourceforge.net/projects/dxflib/

#### 集成方式A：直接编译源文件（推荐）

```batch
# 1. 解压dxflib到项目目录
unzip dxflib.zip -d dxflib

# 2. 编辑 HydraulicCADPlatform.pro，添加：
SOURCES += $$PWD/dxflib/src/dl_dxf.cpp \
           $$PWD/dxflib/src/dl_writer_ascii.cpp
HEADERS += $$PWD/dxflib/src/dl_dxf.h \
           $$PWD/dxflib/src/dl_writer.h \
           $$PWD/dxflib/src/dl_writer_ascii.h
```

#### 集成方式B：使用预编译库

```batch
# 编辑 HydraulicCADPlatform.pro：
INCLUDEPATH += $$PWD/dxflib/src
LIBS += -L$$PWD/dxflib -ldxflib
```

#### 补全代码

编辑 `DXFWrapper.cpp`，将注释的示例代码取消注释，替换mock实现。

### 2. QtXlsx - Excel导出

#### 下载
- https://github.com/dbarabanov/QtXlsxWriter

#### 集成

```batch
# 1. 克隆到项目目录
git clone https://github.com/dbarabanov/QtXlsxWriter.git QtXlsx

# 2. 编辑 HydraulicCADPlatform.pro：
include($$PWD/QtXlsx/src/xlsx/qtxlsx.pri)
```

#### 补全代码

编辑 `ExcelExporter.cpp`，取消注释示例代码。

### 3. Boost.Geometry - 精确几何运算

#### 下载
- https://www.boost.org/

#### 集成

```batch
# 编辑 HydraulicCADPlatform.pro：
INCLUDEPATH += C:\boost_1_84_0

# 使用时添加头文件：
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
```

---

## 编译测试脚本

项目提供两个编译测试脚本：

### compile_test.bat (Windows)

```batch
cd D:\Qt自动算量平台
compile_test.bat
```

功能：
- 检查Qt环境
- 检查项目文件完整性
- 创建构建目录
- 运行qmake
- 编译项目
- 报告结果

### compile_test.sh (Linux/macOS)

```bash
cd /path/to/project
chmod +x compile_test.sh
./compile_test.sh
```

---

## 常见问题

### Q1: "qmake not found"

**原因**: Qt未安装或环境变量未设置

**解决**:
```batch
# Windows - 设置Qt路径
set PATH=C:\Qt\6.x\msvc2019_64\bin;%PATH%

# 或在Qt Creator中打开项目
```

### Q2: 编译错误 "Cannot find file"

**原因**: 项目文件未包含所有源文件

**解决**: 检查 `HydraulicCADPlatform.pro` 中的 SOURCES 和 HEADERS 列表

### Q3: dxflib/QtXlsx相关错误

**原因**: 第三方库未安装

**解决**: 
- 当前 `DXFWrapper.cpp` 和 `ExcelExporter.cpp` 使用mock实现
- 框架代码仍可编译运行
- 如需完整功能，按上述指南集成第三方库

### Q4: "C++17 feature not supported"

**原因**: 编译器版本过低

**解决**: 更新编译器或Qt版本

### Q5: 运行时无DXF/Excel功能

**原因**: 使用mock实现

**解决**: 按第三方库集成指南补全代码

---

## 文件结构

```
D:\Qt自动算量平台\
├── HydraulicCADPlatform.pro   # Qt项目文件
├── compile_test.bat           # Windows编译测试脚本
├── compile_test.sh            # Linux编译测试脚本
│
├── 核心头文件 (14个)
│   ├── Config.h
│   ├── Geometry.h
│   ├── LineUtils.h
│   ├── EnvelopeGenerator.h
│   ├── RulerDetector.h
│   ├── StationMatcher.h
│   ├── VirtualBoxBuilder.h
│   ├── LayerExtractor.h
│   ├── HatchProcessor.h
│   ├── OutputHelper.h
│   ├── EntityHelper.h
│   ├── DXFWrapper.h
│   ├── ExcelExporter.h
│   └── EngineCad.h
│
├── 核心源文件
│   ├── EngineCad.cpp
│   ├── DXFWrapper.cpp
│   └── ExcelExporter.cpp
│
├── UI组件
│   ├── MainWindow.h/cpp
│   ├── FileRowWidget.h/cpp
│   ├── ParamInputWidget.h/cpp
│   ├── ParamSelectWidget.h/cpp
│   ├── ParamCheckboxWidget.h/cpp
│   └── TaskWorker.h/cpp
│
├── 文档
│   ├── BUILD_GUIDE.md
│   ├── DIAGNOSTICS.md
│   ├── REFACTOR_PROGRESS.md
│   └── FINAL_REPORT.md
│
└── 构建输出
    ├── build/               # 构建中间文件
    └── bin/                 # 可执行文件
```

---

## 下一步

1. 运行 `compile_test.bat` 测试编译
2. 如需DXF功能，下载并集成dxflib
3. 如需Excel功能，下载并集成QtXlsx
4. 使用实际DXF文件测试六大任务

---

*构建指南版本: v2.0*
*更新时间: 2026-04-27*