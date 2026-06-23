cmake_minimum_required(VERSION 3.16)
project(HydraulicCADPlatform VERSION 3.7.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ==================== 源文件 ====================
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/EngineCad.cpp
    src/DXFWrapper.cpp
    src/ExcelExporter.cpp
    src/widgets/FileRowWidget.cpp
    src/widgets/ParamInputWidget.cpp
    src/widgets/ParamSelectWidget.cpp
    src/widgets/ParamCheckboxWidget.cpp
    src/workers/TaskWorker.cpp
)

# ==================== 头文件 ====================
set(HEADERS
    src/MainWindow.h
    src/EngineCad.h
    src/DXFWrapper.h
    src/ExcelExporter.h
    include/Config.h
    include/Geometry.h
    src/widgets/FileRowWidget.h
    src/widgets/ParamInputWidget.h
    src/widgets/ParamSelectWidget.h
    src/widgets/ParamCheckboxWidget.h
    src/workers/TaskWorker.h
    include/utils/EntityHelper.h
    include/utils/LineUtils.h
    include/utils/LayerExtractor.h
    include/utils/StationMatcher.h
    include/utils/OutputHelper.h
    include/utils/HatchProcessor.h
    include/utils/EnvelopeGenerator.h
    include/utils/RulerDetector.h
    include/utils/VirtualBoxBuilder.h
)

# ==================== 第三方库 ====================

# dxflib - DXF文件读写库
# 下载: https://sourceforge.net/projects/dxflib/
# 方式: 直接编译源码
option(USE_DXFLIB "Enable dxflib support" ON)
if(USE_DXFLIB)
    set(DXFLIB_DIR ${CMAKE_SOURCE_DIR}/thirdparty/dxflib)
    if(EXISTS ${DXFLIB_DIR}/src)
        set(DXFLIB_SOURCES
            ${DXFLIB_DIR}/src/dl_dxf.cpp
            ${DXFLIB_DIR}/src/dl_writer_ascii.cpp
        )
        set(DXFLIB_HEADERS
            ${DXFLIB_DIR}/src/dl_dxf.h
            ${DXFLIB_DIR}/src/dl_writer.h
            ${DXFLIB_DIR}/src/dl_writer_ascii.h
            ${DXFLIB_DIR}/src/dl_global.h
            ${DXFLIB_DIR}/src/dl_entities.h
            ${DXFLIB_DIR}/src/dl_attributes.h
        )
        include_directories(${DXFLIB_DIR}/src)
        add_definitions(-DUSE_DXFLIB)
    else()
        message(WARNING "dxflib not found at ${DXFLIB_DIR}. Please download from https://sourceforge.net/projects/dxflib/")
    endif()
endif()

# QtXlsx - Excel导出库
# 下载: https://github.com/dbarabanov/QtXlsxWriter
option(USE_QTXLSX "Enable QtXlsx support" OFF)
if(USE_QTXLSX)
    set(QTXLSX_DIR ${CMAKE_SOURCE_DIR}/thirdparty/QtXlsx)
    if(EXISTS ${QTXLSX_DIR})
        include_directories(${QTXLSX_DIR}/src)
        add_definitions(-DUSE_QTXLSX)
    else()
        message(WARNING "QtXlsx not found at ${QTXLSX_DIR}")
    endif()
endif()

# ==================== 可执行文件 ====================
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${DXFLIB_SOURCES}
    ${DXFLIB_HEADERS}
)

# 测试可执行文件
add_executable(TestAutosection
    src/test_autosection.cpp
    src/EngineCad.cpp
    src/DXFWrapper.cpp
    src/ExcelExporter.cpp
    include/Config.h
    include/Geometry.h
    include/utils/LineUtils.h
    include/utils/EnvelopeGenerator.h
    include/utils/RulerDetector.h
    include/utils/StationMatcher.h
    include/utils/OutputHelper.h
    include/utils/HatchProcessor.h
    include/utils/VirtualBoxBuilder.h
    include/utils/LayerExtractor.h
    include/utils/EntityHelper.h
    src/EngineCad.h
    src/DXFWrapper.h
    src/ExcelExporter.h
    ${DXFLIB_SOURCES}
    ${DXFLIB_HEADERS}
)

# DXF图层解析测试
add_executable(TestDxfLayers
    src/test_dxf_layers.cpp
    src/DXFWrapper.cpp
    src/DXFWrapper.h
    include/Geometry.h
    include/Config.h
    include/utils/LineUtils.h
    include/utils/EntityHelper.h
    ${DXFLIB_SOURCES}
    ${DXFLIB_HEADERS}
)

# DXF简化解析测试
add_executable(TestDxfSimple
    src/test_dxf_simple.cpp
)

# 文件读取速度测试
add_executable(TestFileRead
    src/test_file_read.cpp
)

# ==================== 链接库 ====================
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_link_libraries(TestAutosection
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_link_libraries(TestDxfLayers
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_link_libraries(TestDxfSimple
    Qt6::Core
)

target_link_libraries(TestFileRead
    Qt6::Core
)

# V4分层算量测试
add_executable(TestV4
    src/test_v4.cpp
    src/EngineCad.cpp
    src/DXFWrapper.cpp
    src/ExcelExporter.cpp
    include/Config.h
    include/Geometry.h
    include/utils/LineUtils.h
    include/utils/EnvelopeGenerator.h
    include/utils/RulerDetector.h
    include/utils/StationMatcher.h
    include/utils/OutputHelper.h
    include/utils/HatchProcessor.h
    include/utils/VirtualBoxBuilder.h
    include/utils/LayerExtractor.h
    include/utils/EntityHelper.h
    src/EngineCad.h
    src/DXFWrapper.h
    src/ExcelExporter.h
    ${DXFLIB_SOURCES}
    ${DXFLIB_HEADERS}
)

target_link_libraries(TestV4
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(TestV4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(TestV4 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# ==================== 包含目录 ====================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${CMAKE_SOURCE_DIR}/src/workers
)

target_include_directories(TestAutosection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(TestDxfLayers PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(TestDxfSimple PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src
)

# ==================== 输出目录 ====================
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

set_target_properties(TestAutosection PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

set_target_properties(TestDxfLayers PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

set_target_properties(TestDxfSimple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

set_target_properties(TestFileRead PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# ==================== Windows特定设置 ====================
if(WIN32)
    # Windows应用图标
    # set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ==================== 编译定义 ====================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
)

# ==================== 打印配置信息 ====================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Qt6 Found: ${Qt6_FOUND}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "USE_DXFLIB: ${USE_DXFLIB}")
message(STATUS "USE_QTXLSX: ${USE_QTXLSX}")
message(STATUS "========================================")