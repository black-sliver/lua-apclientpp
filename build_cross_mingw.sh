#!/bin/bash

# build helper for cross-compiling with mingw on Linux
# to specify a lua version, pass "luaXX" as first argument
# This is mostly for testing implementations. CI will run on windows.

CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
OS_NAME="windows"
if [ -z "$STD" ]; then
    STD="c++1z"
fi

source ./build_posix.sh
