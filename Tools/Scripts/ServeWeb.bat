@echo off
setlocal

REM Serves a project's web build for local testing.
REM
REM   ServeWeb.bat <project or folder> [port]
REM
REM Serves <project>\Binaries\Web, or the folder itself when it holds an index.html. The port defaults to 8080.
REM HYP_SERVE_WITHOUT_ISOLATION=1 leaves out the COOP / COEP headers, as GitHub Pages does.

set "SERVE_DIR=%~1"
set "PORT=%~2"
if "%PORT%"=="" set "PORT=8080"

if "%SERVE_DIR%"=="" (
    echo Usage: ServeWeb.bat ^<project or folder^> [port]
    exit /b 1
)

if exist "%SERVE_DIR%\Binaries\Web\index.html" set "SERVE_DIR=%SERVE_DIR%\Binaries\Web"

if not exist "%SERVE_DIR%\index.html" (
    echo ERROR: "%SERVE_DIR%" holds no web build. Build one: Tools\Scripts\PackageBuildWeb.bat --project ^<project^>
    exit /b 1
)

echo Serving %SERVE_DIR%
echo Open http://localhost:%PORT%/ in Chrome or Edge. Ctrl+C stops the server.

python "%~dp0ServeWeb.py" "%SERVE_DIR%" "%SERVE_DIR%" %PORT%

endlocal
