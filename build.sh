#!/bin/sh
mkdir -p build
cd build

cmake ../ && cmake --build . -j 4
cd ..


