#!/bin/sh

BUILD_TYPE=${1:-Release}

#if ASAN
if [ "$BUILD_TYPE" = "ASAN" ]; then
    echo "ASAN build"
    echo "symbolizer = $(which llvm-symbolizer)"
    export ASAN_OPTIONS=symbolize=1
    export ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer)
    export CC=clang
    export CXX=clang++
fi
export RUST_BACKTRACE=1
#cmake . -GNinja -B build -D CMAKE_BUILD_TYPE=$BUILD_TYPE
cmake . -G "Unix Makefiles" -B build -D CMAKE_BUILD_TYPE=$BUILD_TYPE
