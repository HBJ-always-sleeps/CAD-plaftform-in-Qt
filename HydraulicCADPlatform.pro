QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 项目名称
TARGET = HydraulicCADPlatform
TEMPLATE = app

# ==================== 源文件 ====================
SOURCES += \
    main.cpp \
    MainWindow.cpp \
    EngineCad.cpp \
    FileRowWidget.cpp \
    ParamInputWidget.cpp \
    ParamSelectWidget.cpp \
    ParamCheckboxWidget.cpp \
    TaskWorker.cpp \
    DXFWrapper.cpp \
    ExcelExporter.cpp

# ==================== 头文件 ====================
HEADERS += \
    MainWindow.h \
    EngineCad.h \
    FileRowWidget.h \
    ParamInputWidget.h \
    ParamSelectWidget.h \
    ParamCheckboxWidget.h \
    TaskWorker.h \
    Config.h \
    Geometry.h \
    EntityHelper.h \
    LineUtils.h \
    LayerExtractor.h \
    StationMatcher.h \
    OutputHelper.h \
    HatchProcessor.h \
    EnvelopeGenerator.h \
    RulerDetector.h \
    VirtualBoxBuilder.h \
    DXFWrapper.h \
    ExcelExporter.h

# ==================== 第三方库 ====================
# DXF库：dxflib（需要自行安装）
# 从 https://sourceforge.net/projects/dxflib/ 下载
# 
# 方式1：使用预编译库
# INCLUDEPATH += $$PWD/dxflib/src
# LIBS += -L$$PWD/dxflib -ldxflib
# 
# 方式2：直接编译源文件（推荐）
# SOURCES += $$PWD/dxflib/src/dl_dxf.cpp \
#            $$PWD/dxflib/src/dl_writer_ascii.cpp
# HEADERS += $$PWD/dxflib/src/dl_dxf.h \
#            $$PWD/dxflib/src/dl_writer.h \
#            $$PWD/dxflib/src/dl_writer_ascii.h \
#            $$PWD/dxflib/src/dl_global.h \
#            $$PWD/dxflib/src/dl_entities.h \
#            $$PWD/dxflib/src/dl_attributes.h

# Excel库：QtXlsx（需要自行安装）
# 从 https://github.com/dbarabanov/QtXlsxWriter 下载
# 
# 方式1：作为子项目
# include($$PWD/QtXlsx/src/xlsx/qtxlsx.pri)
# 
# 方式2：使用预编译库
# INCLUDEPATH += $$PWD/QtXlsx/src
# LIBS += -L$$PWD/QtXlsx -lQtXlsx

# 几何库：Boost.Geometry（可选，用于精确几何运算）
# INCLUDEPATH += /path/to/boost

# ==================== 编译定义 ====================
DEFINES += QT_DEPRECATED_WARNINGS

# 可选：启用dxflib支持
# DEFINES += USE_DXFLIB

# 可选：启用QtXlsx支持
# DEFINES += USE_QTXLSX

# ==================== 输出目录 ====================
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui