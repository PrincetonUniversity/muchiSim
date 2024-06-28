#!/bin/bash
if [ -z "$MUCHI_ROOT" ]; then
    echo "MUCHI_ROOT is not set, run: source setup.sh"
    exit 1
fi
source $MUCHI_ROOT/exp/util.sh;

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
    echo "Error: Missing required parameters."
    exit 1
fi

if [ $1 -gt 8 ]; then
  apps="0 1 2 3 4 5"
else
  apps=$1
fi

if [ -z "$4" ]; then
  let grid_w=64
  echo "Default grid_w=$grid_w"
else
  let grid_w=$4
  echo "grid_w=$grid_w"
fi
if [ -z "$5" ]; then
  echo "All datasets by default"
  datasets="Kron22 wikipedia"
else
  echo "Dataset $5"
  datasets="$5"
fi

if [[ "$datasets" == "Kron16" ]]; then
  set_conf "config_system" "sample_time" 1001
  verbose=2
  # Run in the foreground
  local_run=2
else
  verbose=1
  local_run=$LOCAL_RUN
fi


let lb=$2
let ub=$3
run() {
    local i="$1"
    if [ $lb -le $i ] && [ $ub -ge $i ]; then
        echo "EXPERIMENT: $i"
        exp/run.sh $options -d "$datasets" -a "$apps"
    fi
}

exp="GRANU"

assert=0


let dcache=1
let th=2

prefix="-v $verbose -r $assert -y $dcache -s $local_run"

let torus=1

#0 Torus-32b, SMT=1
let sram_memory=256*512
set_conf "config_system" "smt_per_tile" 1
let chiplet_w=$grid_w/4
options="-n ${exp}0 -t $th $prefix -m $grid_w -u 0 -o $torus -c $chiplet_w -w $sram_memory -k $grid_w"
run 0


#1 Torus-64b, SMT=4
let sram_memory=$sram_memory*4
let grid_w=$grid_w/2
let chiplet_w=$grid_w/4
set_conf "config_system" "smt_per_tile" 4
options="-n ${exp}1 -t $th $prefix -m $grid_w -u 1 -o $torus -c $chiplet_w  -w $sram_memory -k $grid_w"
run 1


#2 Torus-64b 2GHZ, SMT=16
let sram_memory=$sram_memory*4
let grid_w=$grid_w/2
let chiplet_w=$grid_w/4

set_conf "param_energy" "noc_freq" 2.0
set_conf "config_system" "smt_per_tile" 16
options="-n ${exp}2 -t $th $prefix -m $grid_w -u 1 -o $torus -c $chiplet_w  -w $sram_memory -k $grid_w"
run 2

# Reset to the default values of 1GHz and 1 PU/tile
set_conf "param_energy" "noc_freq" 1.0
set_conf "config_system" "smt_per_tile" 1
set_conf "config_system" "sample_time" 1000