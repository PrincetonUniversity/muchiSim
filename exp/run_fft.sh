#!/bin/bash
if [ -z "$MUCHI_ROOT" ]; then
    echo "MUCHI_ROOT is not set, run: source setup.sh"
    exit 1
fi
source $MUCHI_ROOT/exp/util.sh;

app=6

if [ $# -eq 0 ]; then
    echo "Usage: $0 <grid_x>"
    exit 1
fi

let grid_x=$1
let chiplet_w=$grid_x
let pack_w=$grid_x
let proxy_w=$grid_x
let sram_memory=256*48 # 1024 KiB

let dcache=0
let noc_type=0 # NoC of 32 bits
let network=0 # (0: 2D mesh, 1: 2D torus)

# Execution mode (0: normal, 1: dry run)
let dry_run=0
# Functional simulation (0: no, 1: yes)
let func=1
# Run on local machine (0: no, >1: yes)
local_run=$LOCAL_RUN
# Number of threads per core
let th=4

let test=0
let assert_mode=$test
# Level1: print basic stats on each sample, Level2: print full stats on each iteration, Level3: print queue stats, Level 4: print full debug stats
let verbose=1


datasets="fft$func"

name="B3"

cmd="-n ${name} -o $network -t $th -u $noc_type -v $verbose -s $local_run -e $proxy_w -c $chiplet_w -k $pack_w -r $assert_mode -w $sram_memory -y $dcache -l 0 -m $grid_x -p $dry_run -f $func"

exp/run.sh $cmd -d "$datasets" -a "$app"

# check output status
if [ $? -ne 0 ]; then
    echo "ERROR: run.sh failed"
    exit 1
fi

if [ $dry_run -eq 1 ]; then
    echo "DRY RUN COMPLETED!!!"
    exit 0
fi

if [ $local_run -eq 0 ]; then
    echo "PUSHED TO REMOTE MACHINE!"
    exit 0
fi

if [ $func -eq 1 ] && [ $local_run -gt 1 ]; then
    echo "CHECKING OUTPUT..."
    if [ $grid_x -eq 64 ]; then
        DIFF=$(diff output.txt doall/fft_64_ref.txt)
    elif [ $grid_x -eq 32 ]; then
        DIFF=$(diff output.txt doall/fft_32_ref.txt)
    elif [ $grid_x -eq 16 ]; then
        DIFF=$(diff output.txt doall/fft_16_ref.txt)
    else
        echo "NO REFERENCE OUTPUT FOR GRID_X=$grid_x"
        DIFF=""
    fi

    if [ "$DIFF" != "" ]; then
        echo "DIFF!!!"
        #echo "$DIFF"
        exit 1
    else
        echo "SUCCESS!!!"
        exit 0
    fi
fi

