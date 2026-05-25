@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=D:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d D:\Qt自动算量平台

echo ==========================================
echo Qt自动算量平台 - 编译开始
echo ==========================================

echo.
echo [1] 运行 qmake...
qmake HydraulicCADPlatform.pro -spec win32-msvc
if %ERRORLEVEL% neq 0 (
    echo qmake 失败！
    pause
    exit /b 1
)

echo.
echo [2] 编译项目...
nmake
if %ERRORLEVEL% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo.
echo ==========================================
echo 编译成功！
echo ==========================================

if exist bin\HydraulicCADPlatform.exe (
    echo 运行程序: bin\HydraulicCADPlatform.exe
    start bin\HydraulicCADPlatform.exe
) else (
    echo 可执行文件未生成
)

pause