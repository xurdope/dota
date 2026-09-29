@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Spectre.gg ^|^| Dota 2 Skin Changer ^|^| Build
echo ===================================================

set "LOG_FILE=build\build_log.txt"

where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [+] MSVC environment detected.
    goto :BUILD
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] Error: Visual Studio Installer / vswhere.exe not found.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [!] Error: vcvarsall.bat not found in Visual Studio installation path.
    exit /b 1
)

echo [+] Initializing MSVC x64 environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

:BUILD
echo.
echo [+] Preparing build directories...

if not exist "bin" mkdir bin
if not exist "build" mkdir build

if exist "%LOG_FILE%" (
    del "%LOG_FILE%"
    echo [+] Old log deleted.
)

set "CXXFLAGS=/std:c++17 /O2 /LD /W3 /EHa /MD /I src /I imgui /I imgui\backends"
set "LIBS=user32.lib gdi32.lib kernel32.lib winmm.lib d3d11.lib dxgi.lib"

:: Source Files
set "SOURCES=src\DllMain.cpp src\system\system_utils.cpp src\core\memory_guard.cpp src\core\state_manager.cpp src\core\pattern_scan.cpp src\core\skin_changer.cpp src\core\offset_scanner.cpp src\core\econ_hook.cpp src\core\visuals.cpp src\hooks\present_hook.cpp src\ui\dx11_renderer.cpp src\ui\ui_manager.cpp src\ui\menu.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\backends\imgui_impl_win32.cpp imgui\backends\imgui_impl_dx11.cpp"

echo [+] Compiling sources... (Saving log to %LOG_FILE%)

if exist "bin\Spectre.dll" (
    ren "bin\Spectre.dll" "Spectre_old_%RANDOM%.dll" 2>nul
)

cl %CXXFLAGS% %SOURCES% /Fe:bin\Spectre_mod.dll /Fo:build\ /link %LIBS% > "%LOG_FILE%" 2>&1
set COMPILE_STATUS=%ERRORLEVEL%

type "%LOG_FILE%"

if %COMPILE_STATUS% equ 0 (
    copy /Y bin\Spectre_mod.dll bin\Spectre.dll >nul 2>nul
    echo.
    echo ===================================================
    echo   [SUCCESS] Spectre.dll built: bin\Spectre.dll
    echo ===================================================
) else (
    echo.
    echo ===================================================
    echo   [FAILURE] Compilation failed. Check build_log.txt
    echo ===================================================
    exit /b %COMPILE_STATUS%
)

endlocal