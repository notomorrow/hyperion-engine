@echo off

REM Records the engine assets and shaders a project's level uses again, then builds the project for the web into
REM <project>\Binaries\Web. Same as BuildWebGame.ps1 -Record; arguments are passed on to it.
REM
REM   RecordWebAssets.bat -Project <path> [-Worlds <names>] [-RecordSeconds <n>] [-MaxTextureSize <size>] [-FrameRate <n>] [-Run]

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildWebGame.ps1" -Record %*
exit /b %ERRORLEVEL%
