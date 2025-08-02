@echo off

mkdir build\buildtool

pushd build
pushd buildtool

choice /C YN /T 3 /D N /M "Regenerate CMake? (will continue without regenerating in 3s)"
if %errorlevel%==1 (
    cmake ..\..\buildtool
)

cmake --build . --target hyperion-buildtool
if errorlevel 1 (
    exit /b 1
)

if exist hyperion-buildtool.exe (
    echo Found hyperion-buildtool.exe in current directory
) else (
    if exist Debug\hyperion-buildtool.exe (
        echo Found hyperion-buildtool.exe in Debug directory
        move Debug\hyperion-buildtool.exe ..
    ) else (
        if exist Release\hyperion-buildtool.exe (
            echo Found hyperion-buildtool.exe in Release directory
            move Release\hyperion-buildtool.exe ..
        ) else (
            echo Could not find hyperion-buildtool.exe executable!
            exit /b 1
        )
    )
)

popd
popd
