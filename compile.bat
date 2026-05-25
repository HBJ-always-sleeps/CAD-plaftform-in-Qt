@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=D:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d D:\QtCADPlatform\build

echo Compiling...
nmake /f Makefile.Release

if exist release\HydraulicCADPlatform.exe (
    echo Build SUCCESS!
    copy release\HydraulicCADPlatform.exe ..\bin\ /Y
    cd ..
    echo Running application...
    start bin\HydraulicCADPlatform.exe
) else (
    echo Build FAILED or exe not found
)

pause