@echo off

REM create build folder
md build

REM go to build folder
cd build

REM generate makefiles
cmake -G "MSYS Makefiles" ../

REM  run make
make

REM go back
cd ..
