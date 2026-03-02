@echo off
setlocal enabledelayedexpansion

:: Custom Batch Script to Compile C++20 Code with Modules using Dockerized GCC

:: ============================================================================
:: CONFIGURATION
:: ============================================================================
:: Internal Debug Level for the Batch Script (0 = Silent, 1 = Verbose)
set "DEBUG_VAL=1"

set "DOCKER_IMAGE=gcc:latest"
set "TEMP_CPP=obsidian_code.cpp"
set "CONT_WORK_DIR=/usr/src/app"
set "OUT_BIN=program_out"
set "CLEAN_UP=1"
set "GPP_OPTS=-Wall -std=c++20 -fmodules-ts"
set "PREPARE_MODULES=g++ %GPP_OPTS% -x c++-system-header iostream"
:: ============================================================================

set "COMPILER_FLAGS=%GPP_OPTS%"
set "INCLUDE_PARAMS=-I ."
set "DOCKER_VOLUMES=-v "%CD%:%CONT_WORK_DIR%""
set "SRC_FILES=%CONT_WORK_DIR%/%TEMP_CPP%"
set "mode=flags"

if %DEBUG_VAL% GEQ 1 (
    echo ==========================================================
    echo [DOCKER-GCC] BATCH EXECUTION START
    echo ==========================================================
)

:loop
if "%~1" == "" goto end_loop
    set "arg=%~1"
    
    if "!arg!" == "--includes" (
        set "mode=includes"
        goto next
    )

    if "!mode!" == "flags" (
        set "COMPILER_FLAGS=!COMPILER_FLAGS! !arg!"
    ) else (
        if exist "%~1" (
            set "h_dir=%~fp1"
            if "!h_dir:~-1!"=="\" set "h_dir=!h_dir:~0,-1!"
            for %%P in ("!h_dir!\..") do set "p_dir=%%~fP"
            
            set /a rid=!random!
            
            if %DEBUG_VAL% GEQ 1 echo [MOUNT] Found: "!h_dir!"
            
            set "DOCKER_VOLUMES=!DOCKER_VOLUMES! -v "!h_dir!:/libs/inc_!rid!""
            set "INCLUDE_PARAMS=!INCLUDE_PARAMS! -I /libs/inc_!rid!"
            
            if exist "!p_dir!\src" (
                set "DOCKER_VOLUMES=!DOCKER_VOLUMES! -v "!p_dir!\src:/libs/src_!rid!" "
                set "SRC_FILES=!SRC_FILES! /libs/src_!rid!/*.cpp"
            )
            set "SRC_FILES=!SRC_FILES! /libs/inc_!rid!/*.cpp"
        )
    )

:next
shift
goto loop
:end_loop

:: Internal Command Strings
set "COMPILE_CMD=g++ %COMPILER_FLAGS% %INCLUDE_PARAMS% %SRC_FILES% -o %OUT_BIN%"
set "EXEC_CMD=./%OUT_BIN%"
set "CLEAN_CMD=rm %OUT_BIN%"

:: Build the full command for visibility
set "FULL_BASH_CMD=shopt -s nullglob && %PREPARE_MODULES% && %COMPILE_CMD% && %EXEC_CMD%"
if "%CLEAN_UP%"=="1" set "FULL_BASH_CMD=%FULL_BASH_CMD% && %CLEAN_CMD%"

:: ============================================================================
:: SUMMARY & DOCKER COMMAND PREVIEW
:: ============================================================================
if %DEBUG_VAL% GEQ 1 (
    echo ----------------------------------------------------------
    echo [DOCKER-GCC] CONFIGURATION SUMMARY
    echo    Image:         %DOCKER_IMAGE%
    echo    WorkDir:       %CONT_WORK_DIR%
    echo    Source Files:  %SRC_FILES%
    echo ----------------------------------------------------------
    echo [DOCKER-GCC] COMMANDS INSIDE CONTAINER:
    echo    1. Compile:    %COMPILE_CMD%
    echo    2. Execute:    %EXEC_CMD%
    if "%CLEAN_UP%"=="1" echo    3. Cleanup:    %CLEAN_CMD%
    echo ----------------------------------------------------------
    echo [DOCKER-GCC] FULL DOCKER INVOCATION:
    echo    docker run --rm %DOCKER_VOLUMES% -w %CONT_WORK_DIR% %DOCKER_IMAGE% /bin/bash -c "%FULL_BASH_CMD%"
    echo ----------------------------------------------------------
    echo [DEBUG] Performing Container File System Check...
    docker run --rm %DOCKER_VOLUMES% %DOCKER_IMAGE% ls -R /libs %CONT_WORK_DIR%
    echo ----------------------------------------------------------
    echo [DOCKER-GCC] Executing...
    echo.
)

:: ============================================================================
:: ACTUAL EXECUTION
:: ============================================================================
docker run --rm %DOCKER_VOLUMES% -w %CONT_WORK_DIR% %DOCKER_IMAGE% /bin/bash -c "%FULL_BASH_CMD%"
echo.

endlocal