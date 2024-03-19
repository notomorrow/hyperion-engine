#!/bin/sh

echo "Generating project files..."

mkdir -p project

if [[ "$OSTYPE" == "darwin"* ]]; then
    # change to correct path if need be
    cmake -GXcode -B./project -D CMAKE_CXX_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++" -D CMAKE_C_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    cmake -G "Unix Makefiles" -B./project
else
    echo "Unsupported OS"
    exit 1
fi

echo "Project files generated"
