#!/bin/sh
mkdir -p ./build
mkdir -p ./build/res
cp -r ./res/* ./build/*
pushd build
cmake ../
popd
echo "All done. \`cd\` into \`build\` and run \`../build.sh\` to build. Then run \`./hyperion\` to run the engine test."
