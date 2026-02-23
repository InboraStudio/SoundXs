@echo off
setlocal EnableDelayedExpansion

echo ============================================================
echo  SoundXs Build Script  (MinGW + Qt6)
echo  by Inbora
echo ============================================================
echo.

set "QTROOT=D:\QTx\6.10.2\mingw_64"
set "MINGW=D:\QTx\Tools\mingw1310_64\bin"
set "NINJA=D:\QTx\Tools\Ninja"
if not "%~1"=="" set "QTROOT=%~1"
if not exist "%QTROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt6 not found at: %QTROOT%
    echo         Usage: build.bat [path-to-Qt-mingw_64]
    pause & exit /b 1
)
echo [OK] Qt6: %QTROOT%

if not exist "%MINGW%\gcc.exe" (
    echo [ERROR] MinGW not found at: %MINGW%
    pause & exit /b 1
)
echo [OK] MinGW: %MINGW%

if not exist "%NINJA%\ninja.exe" (
    echo [WARN] Ninja not found at %NINJA%, trying system PATH...
    set "NINJA="
)
if defined NINJA echo [OK] Ninja: %NINJA%
set "Qt6_DIR=%QTROOT%\lib\cmake\Qt6"
set "PATH=%MINGW%;%NINJA%;%QTROOT%\bin;%PATH%"

echo.
echo [INFO] Configuring CMake (Release, Ninja, MinGW)...
cmake -B build ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_COMPILER="%MINGW%\g++.exe" ^
    -DCMAKE_C_COMPILER="%MINGW%\gcc.exe" ^
    "-DQt6_DIR=%Qt6_DIR%"

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause & exit /b 1
)

echo.
echo [INFO] Building...
cmake --build build --config Release --parallel

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    pause & exit /b 1
)

echo.
echo ============================================================
echo  [SUCCESS] Build complete!
echo  Executable: build\SoundXs.exe
echo ============================================================
echo.

set /p RUN="Launch SoundXs now? [Y/n] "
if /i not "%RUN%"=="n" (
    start "" "build\SoundXs.exe"
)

endlocal
