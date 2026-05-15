@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: ==============================================================================
:: Manual Build Orchestrator
:: Build UE project for specified platform and configuration
:: ==============================================================================

:: Project directory setup
if "%PROJECT_DIR_OVERRIDE%"=="" (
    set "PROJECT_DIR=%~dp0"
) else (
    set "PROJECT_DIR=%PROJECT_DIR_OVERRIDE%"
)
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

:: Engine directory setup
if "%ENGINE_DIR_OVERRIDE%"=="" (
    set "ENGINE_DIR=F:\wz\UE_CICD\UnrealEngine\UnrealEngine"
) else (
    set "ENGINE_DIR=%ENGINE_DIR_OVERRIDE%"
)
set "UAT_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat"

:: Project info
set "PROJECT_NAME=ExFrameWork"
set "PROJECT_FILE=%PROJECT_DIR%\%PROJECT_NAME%.uproject"
set "ARCHIVE_DIR=%PROJECT_DIR%\Saved\Builds"

:: Validate input parameters
if "%~1"=="" goto Usage
if "%~2"=="" goto Usage

set "TARGET_PLATFORM=%~1"
set "TARGET_CONFIG=%~2"

:: Check for optional flags (3rd arg onwards)
set "CLEAN_FLAG="
set "COOK_CLEAN_FLAG="

for %%A in (%3 %4 %5) do (
    if /i "%%A"=="-clean"      set "CLEAN_FLAG=-clean"
    if /i "%%A"=="-cookclean"  set "COOK_CLEAN_FLAG=1"
)

echo =======================================================
echo [Manual Build Started]
echo Project:  %PROJECT_NAME%
echo Platform: %TARGET_PLATFORM%
echo Config:   %TARGET_CONFIG%
echo Output:   %ARCHIVE_DIR%\%TARGET_PLATFORM%\%TARGET_CONFIG%
if not "%CLEAN_FLAG%"==""      echo Options:  CLEAN BUILD (full rebuild including C++)
if not "%COOK_CLEAN_FLAG%"=="" echo Options:  COOK CLEAN (shader + asset recook only, skip C++ build)
echo =======================================================

:: ──────────────────────────────────────────────────────────────────────────────
:: Dedicated Server Build (Win64Server)
:: UBT로 서버 타겟만 컴파일 — Cook/Stage/Package 없이 바이너리만 생성
:: ──────────────────────────────────────────────────────────────────────────────
if /i "%TARGET_PLATFORM%"=="Win64Server" (
    echo [DedicatedServer] Building %PROJECT_NAME%Server target ^(Win64^)...
    call "%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat" ^
        %PROJECT_NAME%Server ^
        Win64 ^
        %TARGET_CONFIG% ^
        "%PROJECT_FILE%" ^
        -waitmutex ^
        -UBA ^
        %CLEAN_FLAG%

    if !ERRORLEVEL! NEQ 0 (
        echo.
        echo [ERROR] Dedicated Server build failed. Check logs for details.
        exit /b !ERRORLEVEL!
    )

    echo.
    echo [SUCCESS] Dedicated Server build complete^^!
    echo Binaries: "%PROJECT_DIR%\Binaries\Win64\"
    exit /b 0
)

:: ──────────────────────────────────────────────────────────────────────────────
:: Standard Client Build (Win64 / Android / IOS)
:: ──────────────────────────────────────────────────────────────────────────────

:: Run UAT BuildCookRun
:: -cookclean mode: skip C++ compilation entirely, force full recook of shaders and assets
if not "%COOK_CLEAN_FLAG%"=="" (
    call "%UAT_BAT%" BuildCookRun ^
        -project="%PROJECT_FILE%" ^
        -noP4 ^
        -clientconfig="%TARGET_CONFIG%" ^
        -serverconfig="%TARGET_CONFIG%" ^
        -utf8output ^
        -platform="%TARGET_PLATFORM%" ^
        -nocompileeditor -skipbuildeditor -nocompile ^
        -cook -stage -package -archive ^
        -clearcookeddata ^
        -UBA ^
        -archivedirectory="%ARCHIVE_DIR%\%TARGET_PLATFORM%\%TARGET_CONFIG%"
) else (
    call "%UAT_BAT%" BuildCookRun ^
        -project="%PROJECT_FILE%" ^
        -noP4 ^
        -clientconfig="%TARGET_CONFIG%" ^
        -serverconfig="%TARGET_CONFIG%" ^
        -utf8output ^
        -platform="%TARGET_PLATFORM%" ^
        -build -cook -stage -package -archive ^
        -UBA %CLEAN_FLAG% ^
        -archivedirectory="%ARCHIVE_DIR%\%TARGET_PLATFORM%\%TARGET_CONFIG%"
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed. Check logs for details.
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Build and packaging complete!
echo Output: "%ARCHIVE_DIR%\%TARGET_PLATFORM%\%TARGET_CONFIG%"
exit /b 0

:Usage
echo.
echo [Usage]
echo BuildProject.bat ^<Platform^> ^<Configuration^>
echo.
echo Platform: Win64, Android, IOS, Win64Server
echo Configuration: Development, Debug, Test, Shipping
echo.
echo Example:
echo   BuildProject.bat Win64 Development
echo   BuildProject.bat Android Shipping
echo.
exit /b 1
