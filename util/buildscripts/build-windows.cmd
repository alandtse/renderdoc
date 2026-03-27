@echo off
setlocal enabledelayedexpansion

REM Build RenderDoc on Windows without modifying .vcxproj files.
REM Overrides PlatformToolset and Platform via MSBuild command-line properties,
REM so the repo stays clean after building.
REM
REM Usage: build-windows.cmd [Release|Development] [x64|x86]
REM   Default: Development x64
REM
REM Options:
REM   VCVARS64_PATH   Override path to vcvars64.bat

set "CONFIGURATION=%~1"
set "PLATFORM=%~2"
set "SKIP_EXTRAS=0"
if not defined CONFIGURATION set "CONFIGURATION=Development"
if not defined PLATFORM set "PLATFORM=x64"

REM --no-extras skips the 3rdparty UI download (useful for CI/build testing)
for %%A in (%*) do (
    if /i "%%A"=="--no-extras" set "SKIP_EXTRAS=1"
)

REM Validate configuration
if /i not "%CONFIGURATION%"=="Development" if /i not "%CONFIGURATION%"=="Release" (
    echo ERROR: Unknown configuration "%CONFIGURATION%". Use Development or Release.
    exit /b 1
)

REM -----------------------------------------------------------------------
REM Locate Visual Studio via vswhere, then fall back to common paths
REM -----------------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCVARS_PATH="

if exist "%VSWHERE%" (
    for /f "delims=" %%i in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS_PATH=%%i\VC\Auxiliary\Build\vcvars64.bat"
            goto :vcvars_found
        )
    )
)

for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\" (
        for %%E in (Enterprise Professional Community BuildTools) do (
            if exist "%%D:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                set "VCVARS_PATH=%%D:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
                goto :vcvars_found
            )
            if exist "%%D:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                set "VCVARS_PATH=%%D:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat"
                goto :vcvars_found
            )
        )
    )
)

:vcvars_found
if defined VCVARS64_PATH set "VCVARS_PATH=%VCVARS64_PATH%"

if not defined VCVARS_PATH (
    echo ERROR: Could not find vcvars64.bat. Ensure Visual Studio is installed.
    echo Set VCVARS64_PATH to override.
    exit /b 1
)

echo Using: %VCVARS_PATH%
call "%VCVARS_PATH%" x64 >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo ERROR: Failed to initialize Visual Studio environment.
    exit /b 1
)

REM -----------------------------------------------------------------------
REM Detect toolset from the VS installation we just loaded
REM -----------------------------------------------------------------------
set "TOOLSET=v143"
echo %VCVARS_PATH% | findstr /i "2022" >nul && set "TOOLSET=v143"
echo %VCVARS_PATH% | findstr /i "2019" >nul && set "TOOLSET=v142"

REM -----------------------------------------------------------------------
REM Download 3rdparty UI extras (Qt widgets, etc.) if not already present
REM -----------------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\..\"
pushd "%REPO_ROOT%"

if "%SKIP_EXTRAS%"=="0" (
    if not exist "qrenderdoc_3rdparty" (
        echo Downloading 3rdparty UI extras...
        curl -L https://renderdoc.org/qrenderdoc_3rdparty.zip -o qrenderdoc_3rdparty.zip
        if !ERRORLEVEL! NEQ 0 (
            echo ERROR: Failed to download 3rdparty extras.
            popd & exit /b 1
        )
        REM tar is built into Windows 10+ and handles zip files natively
        tar -xf qrenderdoc_3rdparty.zip >nul 2>&1
        if !ERRORLEVEL! NEQ 0 (
            echo WARNING: tar extraction failed, trying 7z...
            7z x qrenderdoc_3rdparty.zip >nul
        )
        del qrenderdoc_3rdparty.zip
    )
) else (
    echo Skipping 3rdparty extras ^(--no-extras^).
)

REM -----------------------------------------------------------------------
REM Build
REM /p:PlatformToolset and /p:WindowsTargetPlatformVersion override the
REM values in .vcxproj without modifying those files. WindowsSDKTarget.props
REM (imported by every project) already detects the SDK version at build
REM time, so WindowsTargetPlatformVersion here is just a safety net.
REM -----------------------------------------------------------------------
echo.
echo ============================================================
echo  RenderDoc  ^|  %CONFIGURATION%  ^|  %PLATFORM%  ^|  %TOOLSET%
echo ============================================================
echo.

msbuild.exe renderdoc.sln ^
    /m ^
    /p:Configuration=%CONFIGURATION% ^
    /p:Platform=%PLATFORM% ^
    /p:PlatformToolset=%TOOLSET% ^
    /p:WindowsTargetPlatformVersion=10.0 ^
    /v:minimal

if !ERRORLEVEL! EQU 0 (
    echo.
    echo Build succeeded: %PLATFORM%\%CONFIGURATION%\
) else (
    echo.
    echo Build FAILED.
    popd & exit /b 1
)

popd
exit /b 0
