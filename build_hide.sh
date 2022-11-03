#!/bin/bash -x

test "$1" = "debug" \
    && CUDA_FLAGS='-g -G' \
    && GCC_FLAGS='-g' \
    && echo "debug mode"

nvcc $CUDA_FLAGS -c libhidecuda.cu -o libhidecuda.o -I/usr/include/opencv
g++ $GCC_FLAGS -c Hide.cpp -o Hide.o -I/usr/include/opencv4
g++ $GCC_FLAGS -o Hide Hide.o libhide.hpp libhidecuda.o \
    `pkg-config opencv4 --cflags --libs` -L/usr/local/cuda/lib64 -lcuda -lcudart

