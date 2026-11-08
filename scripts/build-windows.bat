@echo off
setlocal enabledelayedexpansion

echo === StrataLauncher Windows Build Script (Batch) ===

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: cmake not found in PATH.
    exit /b 1
)

where cpack >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: cpack not found in PATH.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
exit /b %errorlevel%
