@echo off

REM Set the path to the directory containing the project files
set project_dir=./projects

REM Create the projects directory if it doesn't exist
if not exist %project_dir% mkdir %project_dir%

REM Generate the project files for VS using CMake
cmake -G "Visual Studio 16 2019" -A x64 -S . -B %project_dir%
