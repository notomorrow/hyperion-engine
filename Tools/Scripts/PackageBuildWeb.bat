@echo off
setlocal

REM Builds a project for the browser into <project>\Binaries\Web, a folder any static host can serve as it is.
REM Intermediates go to <project>\Build\Web. Tools\Scripts\BuildWebGame.ps1 does the work; this keeps the old name.
REM
REM   PackageBuildWeb.bat --project <path> [--worlds <names>] [--max-texture-size <size>] [--run] [--fast-link]

set "BUILD_ARGS="

:PARSE_ARGS
if "%~1"=="" goto END_PARSE_ARGS

if /I "%~1"=="--project" goto ARG_PROJECT
if /I "%~1"=="--worlds" goto ARG_WORLDS
if /I "%~1"=="--max-texture-size" goto ARG_MAX_TEXTURE_SIZE
if /I "%~1"=="--run" set "BUILD_ARGS=%BUILD_ARGS% -Run"
if /I "%~1"=="--fast-link" set "BUILD_ARGS=%BUILD_ARGS% -FastLink"
shift
goto PARSE_ARGS

:ARG_PROJECT
set "BUILD_ARGS=%BUILD_ARGS% -Project "%~2""
shift
shift
goto PARSE_ARGS

:ARG_WORLDS
set "BUILD_ARGS=%BUILD_ARGS% -Worlds "%~2""
shift
shift
goto PARSE_ARGS

:ARG_MAX_TEXTURE_SIZE
set "BUILD_ARGS=%BUILD_ARGS% -MaxTextureSize %~2"
shift
shift
goto PARSE_ARGS

:END_PARSE_ARGS

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildWebGame.ps1" %BUILD_ARGS%
exit /b %ERRORLEVEL%
