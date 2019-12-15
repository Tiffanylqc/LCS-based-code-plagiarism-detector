# ppa-detector

## Introduction
ppa-detector tries to detect plagiarism in short programs like those in  programming assignments. 

So far it has 2 methods on measuring the similarity of programs:
+ Compute the histograms of different kinds of instructions statically and compare them with chi-squared distance. This method is demonstrative and not reliable.
+ Identify semantically similar basic blocks with sampling and compute the longest common subsequence. 

## Build
    mkdir build && cd build
    cmake -DLLVM_DIR=<path/to/llvm> ..
    make

## Running
Running the detector with instruction histogram based method:

    ./bin/ppa-detector --instruction-histogram <plaintiff>.bc <suspicious>.bc

Running the detector with sampling based dynamic analysis method:

    ./bin/ppa-detector --sebb <plaintiff>.bc <suspicious>.bc </path/to/input/folder> --relocation-model=pic
