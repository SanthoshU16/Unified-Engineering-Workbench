@echo off
setlocal
echo ========================================================
echo       Starting Unified Engineering Workbench...
echo ========================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

if exist "dist\windows-x64\workbench_server.exe" (
    start "" "dist\windows-x64\workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
    goto launched
)

where cmake >nul 2>&1
if not errorlevel 1 goto build_source

if exist "workbench_server.exe" (
    start "" "workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
    goto launched
)
if exist "build\src\backend\Release\workbench_server.exe" (
    start "" "build\src\backend\Release\workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
    goto launched
)
if exist "build\src\backend\workbench_server.exe" (
    start "" "build\src\backend\workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
    goto launched
)
echo [Error] No packaged runtime or usable CMake source build is available.
pause
exit /b 1

:build_source
echo [Development Mode] Checking source and rebuilding changed backend files...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto build_error
cmake --build build --config Release
if errorlevel 1 goto build_error
if exist "build\src\backend\Release\workbench_server.exe" (
    start "" "build\src\backend\Release\workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
) else if exist "build\src\backend\workbench_server.exe" (
    start "" "build\src\backend\workbench_server.exe" --port 8080 --web-root "%SCRIPT_DIR%"
) else (
    echo [Error] Could not launch workbench_server.exe.
    pause
    exit /b 1
)

:launched

echo Workbench launched successfully. Browser will open automatically.
timeout /t 3 /nobreak >nul
exit /b 0

:build_error
echo [Error] Source build failed. Install CMake 3.20+ and a C++17 compiler, or use the packaged Windows x64 runtime.
pause
exit /b 1

