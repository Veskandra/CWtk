@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 goto no_gcc
where windres >nul 2>nul
if errorlevel 1 goto no_windres

where mingw32-make >nul 2>nul
if not errorlevel 1 goto use_mingw_make
where make >nul 2>nul
if not errorlevel 1 goto use_make

echo [WinTk] Neither mingw32-make nor make was found on PATH.
echo Use CMake instead, or add a make executable to PATH.
exit /b 1

:use_mingw_make
mingw32-make %*
exit /b %errorlevel%

:use_make
make %*
exit /b %errorlevel%

:no_gcc
echo [WinTk] gcc was not found on PATH.
echo Install MinGW-w64 and add its bin directory to PATH, or use CMake.
exit /b 1

:no_windres
echo [WinTk] windres was not found on PATH.
echo A complete MinGW-w64 toolchain is required.
exit /b 1
