@echo off
REM 编译脚本 - 航道断面算量自动化平台
REM 使用 Qt 6.8.3 MSVC 2022 编译

REM 设置 Qt 环境
set QTDIR=D:\Qt\6.8.3\msvc2022_64
set PATH=%QTDIR%\bin;%PATH%

REM 设置 MSVC 环境 (如果需要)
REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM 切换到项目目录
cd /d D:\QtCADPlatform

REM 使用 qmake 生成 Makefile
echo Running qmake...
%QTDIR%\bin\qmake.exe HydraulicCADPlatform.pro -spec win32-msvc

REM 编译
echo Building...
nmake

REM 检查结果
if exist bin\HydraulicCADPlatform.exe (
    echo Build successful!
    echo Output: bin\HydraulicCADPlatform.exe
) else (
    echo Build failed!
)

pause