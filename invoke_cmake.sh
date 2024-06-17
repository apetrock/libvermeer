#!/bin/sh

BUILD_TYPE=${1:-release}

#cmake . -GNinja -B build -D CMAKE_BUILD_TYPE=$BUILD_TYPE
cmake . -G "Unix Makefiles" -B build -D CMAKE_BUILD_TYPE=$BUILD_TYPE
