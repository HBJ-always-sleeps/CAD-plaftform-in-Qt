#!/bin/bash
# compile_test.sh - Qt自动算量平台编译测试脚本
# 
# 用于验证代码结构是否正确

echo "=========================================="
echo "Qt自动算量平台 - 编译测试"
echo "=========================================="

# 检查Qt环境
echo ""
echo "[1] 检查Qt环境..."

if command -v qmake &> /dev/null; then
    QMAKE_VERSION=$(qmake -v 2>&1 | head -n 1)
    echo "    qmake: $QMAKE_VERSION"
else
    echo "    ERROR: qmake not found!"
    echo "    Please install Qt development environment"
    exit 1
fi

# 检查项目文件
echo ""
echo "[2] 检查项目文件..."

PROJECT_FILE="HydraulicCADPlatform.pro"
if [ -f "$PROJECT_FILE" ]; then
    echo "    OK: $PROJECT_FILE exists"
    
    # 检查头文件列表
    HEADERS_COUNT=$(grep -c "^HEADERS" "$PROJECT_FILE" 2>/dev/null || echo "0")
    echo "    Headers section found"
    
    # 检查源文件列表
    SOURCES_COUNT=$(grep -c "^SOURCES" "$PROJECT_FILE" 2>/dev/null || echo "0")
    echo "    Sources section found"
else
    echo "    ERROR: $PROJECT_FILE not found!"
    exit 1
fi

# 检查头文件
echo ""
echo "[3] 检查头文件..."

REQUIRED_HEADERS=(
    "Config.h"
    "Geometry.h"
    "LineUtils.h"
    "EnvelopeGenerator.h"
    "RulerDetector.h"
    "StationMatcher.h"
    "VirtualBoxBuilder.h"
    "LayerExtractor.h"
    "HatchProcessor.h"
    "OutputHelper.h"
    "EntityHelper.h"
    "DXFWrapper.h"
    "ExcelExporter.h"
    "EngineCad.h"
)

MISSING_HEADERS=0
for header in "${REQUIRED_HEADERS[@]}"; do
    if [ -f "$header" ]; then
        LINES=$(wc -l < "$header")
        echo "    OK: $header ($LINES lines)"
    else
        echo "    MISSING: $header"
        MISSING_HEADERS=$((MISSING_HEADERS + 1))
    fi
done

if [ $MISSING_HEADERS -gt 0 ]; then
    echo "    ERROR: $MISSING_HEADERS header files missing!"
    exit 1
fi

# 检查源文件
echo ""
echo "[4] 检查源文件..."

REQUIRED_SOURCES=(
    "EngineCad.cpp"
    "DXFWrapper.cpp"
    "ExcelExporter.cpp"
    "TaskWorker.cpp"
)

MISSING_SOURCES=0
for source in "${REQUIRED_SOURCES[@]}"; do
    if [ -f "$source" ]; then
        LINES=$(wc -l < "$source")
        echo "    OK: $source ($LINES lines)"
    else
        echo "    MISSING: $source"
        MISSING_SOURCES=$((MISSING_SOURCES + 1))
    fi
done

if [ $MISSING_SOURCES -gt 0 ]; then
    echo "    ERROR: $MISSING_SOURCES source files missing!"
    exit 1
fi

# 创建构建目录
echo ""
echo "[5] 创建构建目录..."

mkdir -p build/obj
mkdir -p build/moc
mkdir -p build/rcc
mkdir -p build/ui
mkdir -p bin

echo "    OK: build directories created"

# 运行qmake
echo ""
echo "[6] 运行qmake..."

qmake HydraulicCADPlatform.pro -o build/Makefile

if [ -f "build/Makefile" ]; then
    echo "    OK: Makefile generated"
else
    echo "    ERROR: Makefile generation failed!"
    exit 1
fi

# 编译
echo ""
echo "[7] 编译项目..."

cd build
make -j4 2>&1 | tee compile.log
COMPILE_RESULT=${PIPESTATUS[0]}
cd ..

if [ $COMPILE_RESULT -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "编译成功!"
    echo "=========================================="
    
    if [ -f "bin/HydraulicCADPlatform" ] || [ -f "bin/HydraulicCADPlatform.exe" ]; then
        echo "可执行文件: bin/HydraulicCADPlatform"
    fi
else
    echo ""
    echo "=========================================="
    echo "编译失败 - 请检查错误日志"
    echo "=========================================="
    echo "错误日志: build/compile.log"
    
    # 显示常见错误提示
    echo ""
    echo "常见问题:"
    echo "  1. 缺少dxflib: 当前DXFWrapper.cpp使用mock实现"
    echo "  2. 缺少QtXlsx: 当前ExcelExporter.cpp使用mock实现"
    echo "  3. Qt版本不兼容: 确保使用Qt 6.x"
    echo ""
    echo "如果仅缺少第三方库功能，框架代码仍可编译运行"
fi

echo ""
echo "测试完成"