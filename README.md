# Code Plagiarism Detector

## Introduction
This Longest Common Subsequence based detector tries to detect plagiarism in short programs like programming assignments. It considers similarity for both basic blocks and code paths. Check report [here](https://drive.google.com/file/d/1TSfQDWxjvaZfGX5b9TX6fmgHVXwXQRti/view?usp=sharing)


## Build
    mkdir build && cd build
    cmake -DLLVM_DIR=<path/to/llvm> ..
    make

## Running
Running the detector with instruction histogram based method:

    ./bin/ppa-detector --instruction-histogram <plaintiff>.bc <suspicious>.bc

Running the detector with sampling based dynamic analysis method:

    ./bin/ppa-detector --sebb <plaintiff>.bc <suspicious>.bc </path/to/input/folder> --relocation-model=pic
