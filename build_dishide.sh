#!/bin/bash -x

test "$1" = "debug" \
    && CUDA_FLAGS='-g -G' \
    && GCC_FLAGS='-g' \
    && echo "debug mode"

nvcc $CUDA_FLAGS -c libhidecuda.cu -o libhidecuda.o
g++ $GCC_FLAGS -c -o Dishide.o Dishide.cpp -I/usr/include/opencv4
g++ $GCC_FLAGS -o Dishide Dishide.o libhide.hpp libhidecuda.o \
    `pkg-config opencv4 --cflags --libs` -L/usr/local/cuda/lib64 -lcuda -lcudart
