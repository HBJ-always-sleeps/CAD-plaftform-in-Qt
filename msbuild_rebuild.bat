@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\QtCADPlatform\build

echo Building TestAutosection with MSBuild...
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" TestAutosection.vcxproj /p:Configuration=Debug /verbosity:minimal

if exist Debug\TestAutosection.exe (
    echo Build SUCCESS
    copy Debug\TestAutosection.exe ..\bin\Debug\ /Y
) else (
    if exist x64\Debug\TestAutosection.exe (
        echo Build SUCCESS
        copy x64\Debug\TestAutosection.exe ..\bin\Debug\ /Y
    ) else (
        echo Build FAILED
    )
)