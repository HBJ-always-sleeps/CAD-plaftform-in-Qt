@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\Qt自动算量平台
D:\Qt\6.8.3\msvc2022_64\bin\qmake.exe HydraulicCADPlatform.pro
nmake