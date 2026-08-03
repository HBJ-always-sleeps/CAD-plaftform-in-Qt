cmake_minimum_required(VERSION 3.16)
project(HydraulicCADPlatform VERSION 4.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    resources/scripts.qrc
    src/main.cpp
    src/MainWindow.cpp
    src/EngineCad.cpp
    src/DXFWrapper.cpp
    src/ExcelExporter.cpp
    src/PythonBridge.cpp
    src/widgets/FileRowWidget.cpp
    src/widgets/ParamInputWidget.cpp
    src/widgets/ParamSelectWidget.cpp
    src/widgets/ParamCheckboxWidget.cpp
    src/workers/TaskWorker.cpp
)

set(HEADERS
    src/MainWindow.h
    src/EngineCad.h
    src/DXFWrapper.h
    src/ExcelExporter.h
    src/PythonBridge.h
    include/Config.h
    include/Geometry.h
    src/widgets/FileRowWidget.h
    src/widgets/ParamInputWidget.h
    src/widgets/ParamSelectWidget.h
    src/widgets/ParamCheckboxWidget.h
    src/workers/TaskWorker.h
    include/utils/LineUtils.h
    include/utils/StationMatcher.h
    include/utils/EnvelopeGenerator.h
    include/utils/VirtualBoxBuilder.h
)

option(USE_DXFLIB "Enable dxflib support" ON)
if(USE_DXFLIB)
    set(DXFLIB_DIR ${CMAKE_SOURCE_DIR}/../thirdparty/dxflib)
    if(EXISTS ${DXFLIB_DIR}/src)
        set(DXFLIB_SOURCES ${DXFLIB_DIR}/src/dl_dxf.cpp ${DXFLIB_DIR}/src/dl_writer_ascii.cpp)
        set(DXFLIB_HEADERS ${DXFLIB_DIR}/src/dl_dxf.h ${DXFLIB_DIR}/src/dl_writer.h ${DXFLIB_DIR}/src/dl_writer_ascii.h ${DXFLIB_DIR}/src/dl_global.h ${DXFLIB_DIR}/src/dl_entities.h ${DXFLIB_DIR}/src/dl_attributes.h)
        include_directories(${DXFLIB_DIR}/src)
        add_definitions(-DUSE_DXFLIB)
    else()
        message(WARNING "dxflib not found at ${DXFLIB_DIR}")
    endif()
endif()

option(USE_QTXLSX "Enable QtXlsx support" OFF)
if(USE_QTXLSX)
    set(QTXLSX_DIR ${CMAKE_SOURCE_DIR}/../thirdparty/QtXlsx)
    if(EXISTS ${QTXLSX_DIR})
        include_directories(${QTXLSX_DIR}/src)
        add_definitions(-DUSE_QTXLSX)
    endif()
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${DXFLIB_SOURCES} ${DXFLIB_HEADERS})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/excel_migrate.py"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts/excel_migrate.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/autosection_compute.py"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts/autosection_compute.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/autopaste_apply.py"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts/autopaste_apply.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/geology_topview_v41.py"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts/geology_topview_v41.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/geology_topview_section_connect_debug.py"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts/geology_topview_section_connect_debug.py"
    COMMENT "Copying Python runtime scripts"
)

set(TEST_COMMON_SOURCES
    resources/scripts.qrc
    src/EngineCad.cpp src/DXFWrapper.cpp src/ExcelExporter.cpp src/PythonBridge.cpp
)
set(TEST_COMMON_HEADERS
    include/Config.h include/Geometry.h include/utils/LineUtils.h include/utils/EnvelopeGenerator.h
    include/utils/StationMatcher.h include/utils/VirtualBoxBuilder.h
    src/EngineCad.h src/DXFWrapper.h src/ExcelExporter.h src/PythonBridge.h
)

add_executable(TestAutosection
    src/test_autosection.cpp ${TEST_COMMON_SOURCES} ${TEST_COMMON_HEADERS}
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)

add_executable(TestDxfLayers
    src/test_dxf_layers.cpp src/DXFWrapper.cpp src/DXFWrapper.h
    include/Geometry.h include/Config.h include/utils/LineUtils.h
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)

add_executable(TestDxfSimple src/test_dxf_simple.cpp)
add_executable(TestFileRead src/test_file_read.cpp)

add_executable(TestAutopaste
    src/test_autopaste.cpp ${TEST_COMMON_SOURCES} ${TEST_COMMON_HEADERS}
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)

add_executable(TestV4
    src/test_v4.cpp ${TEST_COMMON_SOURCES} ${TEST_COMMON_HEADERS}
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)

add_executable(TestExcelMigration
    src/test_excel_migration.cpp ${TEST_COMMON_SOURCES} ${TEST_COMMON_HEADERS}
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)

add_executable(TestGeologyTopview
    src/test_geology_topview.cpp ${TEST_COMMON_SOURCES} ${TEST_COMMON_HEADERS}
    ${DXFLIB_SOURCES} ${DXFLIB_HEADERS}
)
add_custom_command(TARGET TestGeologyTopview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestGeologyTopview>/scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/geology_topview_v41.py"
            "$<TARGET_FILE_DIR:TestGeologyTopview>/scripts/geology_topview_v41.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../scripts/geology_topview_section_connect_debug.py"
            "$<TARGET_FILE_DIR:TestGeologyTopview>/scripts/geology_topview_section_connect_debug.py"
    COMMENT "Copying v4.1 geology top-view test script"
)

target_link_libraries(${PROJECT_NAME} Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestAutosection Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestDxfLayers Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestDxfSimple Qt6::Core)
target_link_libraries(TestFileRead Qt6::Core)
target_link_libraries(TestAutopaste Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestV4 Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestExcelMigration Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(TestGeologyTopview Qt6::Core Qt6::Gui Qt6::Widgets)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/utils ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/widgets ${CMAKE_SOURCE_DIR}/src/workers
)
target_include_directories(TestAutosection PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestDxfLayers PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestDxfSimple PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestAutopaste PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestV4 PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestExcelMigration PRIVATE ${TEST_INCLUDE_DIRS})
target_include_directories(TestGeologyTopview PRIVATE ${TEST_INCLUDE_DIRS})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestAutosection PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestDxfLayers PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestDxfSimple PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestFileRead PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestAutopaste PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestV4 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(TestExcelMigration PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_DEPRECATED_WARNINGS)

message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION} (V2)")
message(STATUS "Qt6 Found: ${Qt6_FOUND}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")
