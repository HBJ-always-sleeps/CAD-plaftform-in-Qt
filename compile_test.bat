@echo off
REM compile_test.bat - Qt自动算量平台编译测试脚本 (Windows)
REM 
REM 用于验证代码结构是否正确

echo ==========================================
echo Qt自动算量平台 - 编译测试
echo ==========================================

cd /d D:\Qt自动算量平台

REM 检查Qt环境
echo.
echo [1] 检查Qt环境...

where qmake >nul 2>&1
if %ERRORLEVEL% equ 0 (
    for /f "tokens=*" %%i in ('qmake -v 2^>^&1 ^| findstr /n "." ^| findstr "1:"') do (
        echo     qmake: %%i
    )
) else (
    echo     ERROR: qmake not found!
    echo     Please install Qt development environment
    echo     或设置Qt环境变量路径
    pause
    exit /b 1
)

REM 检查项目文件
echo.
echo [2] 检查项目文件...

if exist HydraulicCADPlatform.pro (
    echo     OK: HydraulicCADPlatform.pro exists
) else (
    echo     ERROR: HydraulicCADPlatform.pro not found!
    pause
    exit /b 1
)

REM 检查头文件
echo.
echo [3] 检查头文件...

set MISSING_HEADERS=0

for %%h in (Config.h Geometry.h LineUtils.h EnvelopeGenerator.h RulerDetector.h StationMatcher.h VirtualBoxBuilder.h LayerExtractor.h HatchProcessor.h OutputHelper.h EntityHelper.h DXFWrapper.h ExcelExporter.h EngineCad.h) do (
    if exist %%h (
        echo     OK: %%h
    ) else (
        echo     MISSING: %%h
        set /a MISSING_HEADERS+=1
    )
)

if %MISSING_HEADERS% gtr 0 (
    echo     ERROR: %MISSING_HEADERS% header files missing!
    pause
    exit /b 1
)

REM 检查源文件
echo.
echo [4] 检查源文件...

set MISSING_SOURCES=0

for %%s in (EngineCad.cpp DXFWrapper.cpp ExcelExporter.cpp TaskWorker.cpp) do (
    if exist %%s (
        echo     OK: %%s
    ) else (
        echo     MISSING: %%s
        set /a MISSING_SOURCES+=1
    )
)

if %MISSING_SOURCES% gtr 0 (
    echo     ERROR: %MISSING_SOURCES% source files missing!
    pause
    exit /b 1
)

REM 创建构建目录
echo.
echo [5] 创建构建目录...

if not exist build mkdir build
if not exist build\obj mkdir build\obj
if not exist build\moc mkdir build\moc
if not exist build\rcc mkdir build\rcc
if not exist build\ui mkdir build\ui
if not exist bin mkdir bin

echo     OK: build directories created

REM 运行qmake
echo.
echo [6] 运行qmake...

qmake HydraulicCADPlatform.pro -o build\Makefile

if exist build\Makefile (
    echo     OK: Makefile generated
) else (
    echo     ERROR: Makefile generation failed!
    echo     可能需要指定Qt版本: qmake -spec win32-msvc HydraulicCADPlatform.pro
    pause
    exit /b 1
)

REM 编译
echo.
echo [7] 编译项目...

cd build

REM 尝试使用nmake (MSVC) 或 mingw32-make
where nmake >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo     使用 nmake 编译...
    nmake 2>&1 | tee compile.log
) else (
    where mingw32-make >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        echo     使用 mingw32-make 编译...
        mingw32-make -j4 2>&1 | tee compile.log
    ) else (
        where make >nul 2>&1
        if %ERRORLEVEL% equ 0 (
            echo     使用 make 编译...
            make -j4 2>&1 | tee compile.log
        ) else (
            echo     ERROR: 找不到编译器 (nmake/mingw32-make/make)
            echo     请确保安装了MSVC或MinGW
            cd ..
            pause
            exit /b 1
        )
    )
)

cd ..

if exist bin\HydraulicCADPlatform.exe (
    echo.
    echo ==========================================
    echo 编译成功!
    echo ==========================================
    echo 可执行文件: bin\HydraulicCADPlatform.exe
) else (
    echo.
    echo ==========================================
    echo 编译可能失败 - 请检查错误日志
    echo ==========================================
    echo 错误日志: build\compile.log
    
    echo.
    echo 常见问题:
    echo   1. 缺少dxflib: 当前DXFWrapper.cpp使用mock实现
    echo   2. 缺少QtXlsx: 当前ExcelExporter.cpp使用mock实现
    echo   3. Qt版本不兼容: 确保使用Qt 6.x
    echo.
    echo 如果仅缺少第三方库功能，框架代码仍可编译运行
)

echo.
echo 测试完成
pause