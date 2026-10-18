@echo off
setlocal
if not defined VSCMD_VER call "C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist "build\lanxi-core" mkdir "build\lanxi-core"
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\core_test.cpp src\services\acquisition\lanxi\LanXiCore.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_core_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_core_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\analysis_test.cpp src\services\acquisition\lanxi\LanXiCore.cpp src\services\acquisition\lanxi\LanXiAnalysis.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_analysis_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_analysis_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\advanced_test.cpp src\services\acquisition\lanxi\LanXiCore.cpp src\services\acquisition\lanxi\LanXiAnalysis.cpp src\services\acquisition\lanxi\LanXiAdvanced.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_advanced_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_advanced_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\interchange_test.cpp src\services\acquisition\lanxi\LanXiUff.cpp src\services\acquisition\lanxi\LanXiStreaming.cpp src\services\acquisition\lanxi\LanXiCore.cpp src\services\acquisition\lanxi\LanXiAnalysis.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_interchange_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_interchange_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\modal_test.cpp src\services\acquisition\lanxi\LanXiModal.cpp src\services\acquisition\lanxi\LanXiAdvanced.cpp src\services\acquisition\lanxi\LanXiCore.cpp src\services\acquisition\lanxi\LanXiAnalysis.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_modal_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_modal_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I src\services\acquisition\lanxi tests\lanxi\sound_power_test.cpp src\services\acquisition\lanxi\LanXiSoundPower.cpp /Fo:build\lanxi-core\ /Fe:build\lanxi-core\lanxi_sound_power_test.exe
if errorlevel 1 exit /b 1
build\lanxi-core\lanxi_sound_power_test.exe
set "LANXI_TEST_EXIT=%ERRORLEVEL%"
popd
exit /b %LANXI_TEST_EXIT%
