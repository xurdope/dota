@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Spectre Source 2 Loader Compiler
echo ===================================================

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

if not exist "bin" mkdir bin
if not exist "build" mkdir build

echo [+] Compiling src\loader\main.cpp into bin\loader.exe...
cl /std:c++17 /O2 /W3 /EHa /MD src\loader\main.cpp /Fe:bin\loader.exe /Fo:build\ /link user32.lib advapi32.lib shell32.lib psapi.lib kernel32.lib

if %ERRORLEVEL% equ 0 (
    echo.
    echo ===================================================
    echo   [SUCCESS] bin\loader.exe built successfully.
    echo ===================================================
) else (
    echo.
    echo ===================================================
    echo   [FAILURE] Loader compilation failed.
    echo ===================================================
    exit /b 1
)

endlocal
