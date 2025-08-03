@echo off
REM This script builds the C# libraries manually - Xcode currently will not build them so we need to do it ourselves.

SET CONFIG=Release

REM Override with command line argument if provided
IF NOT "%~1"=="" SET CONFIG=%~1

SET projects=HyperionCore HyperionRuntime HyperionInterop HyperionScripting

pushd build
SET buildDir=%CD%
pushd CSharpProjects

FOR %%p IN (%projects%) DO (
    echo Building %%p...
    
    pushd "%%p"
    dotnet build --disable-build-servers --no-restore --configuration %CONFIG%
    IF %ERRORLEVEL% NEQ 0 (
        echo Failed to build %%p
        exit /b 1
    )
    
    IF NOT EXIST "%buildDir%\bin" mkdir "%buildDir%\bin"
    SET dstPath=%buildDir%\bin\%%p.dll
    SET srcPath=bin\%CONFIG%\net8.0\%%p.dll
    echo copy %srcPath% to %dstPath%
    copy "%srcPath%" "%dstPath%"
    popd
)
popd
popd