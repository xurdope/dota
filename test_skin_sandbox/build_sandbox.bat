@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo  Building Standalone Skin Sandbox Test Suite (MSVC x64)
echo ============================================================

cd /d "%~dp0"

where cl >nul 2>&1
if %ERRORLEVEL% equ 0 goto :COMPILE

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" goto :CHECK_CL

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH goto :CHECK_CL

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [+] Initializing MSVC x64 environment...
    call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
)

:CHECK_CL
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] cl.exe not found. Please run from Visual Studio Developer Command Prompt.
    exit /b 1
)

:COMPILE
echo [*] Compiling sandbox_manager.cpp...
cl /nologo /EHsc /W4 /std:c++17 /Fe:sandbox_manager.exe sandbox_manager.cpp
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile sandbox_manager.cpp
    exit /b %ERRORLEVEL%
)

echo [*] Compiling sandbox_skin_module.cpp...
cl /nologo /EHsc /W4 /std:c++17 /Fe:sandbox_skin_module.exe sandbox_skin_module.cpp
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile sandbox_skin_module.cpp
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  [SUCCESS] Built test utilities successfully:
echo          - sandbox_manager.exe
echo          - sandbox_skin_module.exe
echo ============================================================

endlocal
