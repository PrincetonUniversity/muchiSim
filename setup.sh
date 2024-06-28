#!/bin/bash
mkdir -p sim_counters
mkdir -p plots/characterization
mkdir -p plots/images
export MUCHI_ROOT=$(pwd)

# Check content of CXX variable and set it to a default value if not set
if [ -z "$CXX" ]; then
    echo "Default C Compiler CXX is not set."
    # Check if g++ is installed
    if which icpx > /dev/null 2>&1; then
        # module load intel/2021.1.2
        echo "SUCCESS: icpx detected... setting it as default CXX. Asuming Intel CPU"
        export CXX=icpx
    elif which g++ > /dev/null 2>&1; then
        echo "SUCCESS: g++ detected... setting it as default CXX."
        export CXX=g++
    elif which clang++ > /dev/null 2>&1; then
        echo "SUCCESS: clang++ detected... setting it as default CXX."
        export CXX=clang++
    else
        echo "ERROR: C++ compiler not detected... please install set CXX to a valid C++ compiler, e.g. g++, clang++ or icpx (Intel-only)"
    fi
else
    echo "Compiler $CXX set in CXX."
    if which $CXX > /dev/null 2>&1; then
        echo "SUCCESS: Compiler $CXX detected."
    else
        echo "ERROR: Compiler $CXX not found... please set CXX to a valid C++ compiler, e.g. g++, clang++ or icpx (Intel-only)"
    fi
fi

if which sbatch > /dev/null 2>&1; then
    export LOCAL_RUN=0
else
    export LOCAL_RUN=1
fi
source $MUCHI_ROOT/exp/util.sh;