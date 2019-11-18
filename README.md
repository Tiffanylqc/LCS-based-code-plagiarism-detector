# ppa-detector

## Build
    mkdir build && cd build
    CC=<path to clang> CXX=<path to clang++> cmake -DLLVM_DIR=<path to llvm> ..
    make

## Running
Running the detector with instruction histogram based metrics:

    ./bin/ppa-detector --instruction-histogram <plaintiff>.bc <suspicious>.bc
