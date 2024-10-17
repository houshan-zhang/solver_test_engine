#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
cd $BASEDIR
if [ ! -d src ]
then
    echo "ERROR: can not find $BASEDIR/src directory."
    exit -1
fi
which cmake
cmake --version 2>&1
tmp=$?
if [ ${tmp} -ne 0 ]
then
    echo "ERROR: You should install cmake(2.8 or later) first."
    echo "Please goto https://cmake.org to download and install it."
    exit -1
fi

rm -rf build
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Debug

tmp=$?
echo "cmake compile return:" ${tmp}
if [ ${tmp} -ne 0 ]
then
    exit -1
fi

make
tmp=$?
echo "make compile return:" ${tmp}
if [ ${tmp} -ne 0 ]
then
    exit -1
fi
