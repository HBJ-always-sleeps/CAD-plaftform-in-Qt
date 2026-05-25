@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set QTDIR=D:\Qt\6.8.3\msvc2022_64
set PATH=%QTDIR%\bin;%PATH%
cd /d D:\QtCADPlatform\build

echo Compiling DXFWrapper.cpp...
cl.exe /c /nologo /Zi /MD /EHsc /W3 /DQT_CORE_LIB /DQT_WIDGETS_LIB /DQT_GUI_LIB ..\src\DXFWrapper.cpp /I..\include /I%QTDIR%\include /I%QTDIR%\include\QtCore /I%QTDIR%\include\QtWidgets /I%QTDIR%\include\QtGui /I%QTDIR%\include\QtCore\QtCore /I%QTDIR%\include\QtWidgets\QtWidgets /I%QTDIR%\include\QtGui\QtGui /FoDXFWrapper.obj

if exist DXFWrapper.obj (
    echo DXFWrapper.obj created
    echo Linking TestAutosection.exe...
    link.exe /NOLOGO /DEBUG /SUBSYSTEM:CONSOLE DXFWrapper.obj test_autosection.obj EngineCad.obj Geometry.obj Config.obj LineUtils.obj LayerExtractor.obj StationMatcher.obj OutputHelper.obj EntityHelper.obj EnvelopeGenerator.obj HatchProcessor.obj RulerDetector.obj VirtualBoxBuilder.obj %QTDIR%\lib\Qt6Core.lib %QTDIR%\lib\Qt6Widgets.lib %QTDIR%\lib\Qt6Gui.lib /OUT:TestAutosection.exe
    if exist TestAutosection.exe (
        echo Build SUCCESS
        copy TestAutosection.exe ..\bin\Debug\ /Y
    ) else (
        echo Link FAILED
    )
) else (
    echo Compile FAILED
)