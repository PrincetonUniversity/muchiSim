#!/bin/bash
if [ -z "$MUCHI_ROOT" ]; then
    echo "MUCHI_ROOT is not set, run: source setup.sh"
    exit 1
fi
source $MUCHI_ROOT/exp/util.sh;

sim_log="Kron16--16-X-16--BA16-A4"
if [ -z "$1" ]; then
  echo "Missing sim log parameter, re-run with basic sim log: Kron16--16-X-16--BA16-A4"
else
  echo "Sim log: $1"
  sim_log=$1
fi

# Check that the sim_log file exists
if [ ! -f $MUCHI_ROOT/sim_counters/COUNT-$sim_log.log ]; then
  echo "File $MUCHI_ROOT/sim_counters/COUNT-$sim_log.log does not exist"
  exit 1
fi

compiler=${CXX:-g++} # It has been tested with clang++ g++ or icpx (Intel CPUs)

compile="$compiler -std=c++11 $MUCHI_ROOT/src/energy_cost_model.cpp -o $MUCHI_ROOT/bin/model.run"
echo $compile
$compile
$MUCHI_ROOT/bin/model.run $MUCHI_ROOT/sim_counters/COUNT-$sim_log.log 
