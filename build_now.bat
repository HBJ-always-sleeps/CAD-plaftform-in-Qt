@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=D:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d D:\QtCADPlatform

echo ==========================================
echo Qt CAD Platform - Build Start
echo ==========================================

if not exist build mkdir build
if not exist bin mkdir bin

echo.
echo [1] Running qmake...
qmake HydraulicCADPlatform.pro -spec win32-msvc -o build\Makefile
if errorlevel 1 (
    echo qmake FAILED!
    exit /b 1
)
echo qmake SUCCESS!

echo.
echo [2] Compiling...
cd build
nmake /nologo
if errorlevel 1 (
    echo COMPILE FAILED!
    cd ..
    exit /b 1
)
cd ..

echo.
echo ==========================================
echo BUILD SUCCESS!
echo ==========================================

if exist bin\HydraulicCADPlatform.exe (
    echo Starting application...
    start bin\HydraulicCADPlatform.exe
) else (
    echo Executable not found in bin folder
    if exist debug\HydraulicCADPlatform.exe (
        echo Found in debug folder
        start debug\HydraulicCADPlatform.exe
    ) else (
        if exist release\HydraulicCADPlatform.exe (
            echo Found in release folder
            start release\HydraulicCADPlatform.exe
        )
    )
)