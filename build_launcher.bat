@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Spectre Launcher Compiler
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

echo [+] Compiling main.cpp into bin\Launcher.exe...
cl /std:c++17 /O2 /W3 /EHa /MD main.cpp /Fe:bin\Launcher.exe /Fo:build\ /link user32.lib advapi32.lib shell32.lib kernel32.lib

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] bin\Launcher.exe built successfully.
) else (
    echo [FAILURE] Compilation failed.
)

endlocal
