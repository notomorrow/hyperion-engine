#!/bin/bash
mkdir -p build
cd build

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    cmake ../
fi

cmake --build . -j 4

cd ..


