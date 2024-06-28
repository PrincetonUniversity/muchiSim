#!/bin/bash
if [ -z "$MUCHI_ROOT" ]; then
    echo "MUCHI_ROOT is not set, run: source setup.sh"
    exit 1
fi
source $MUCHI_ROOT/exp/util.sh;

# Check if first argument is provided, otherwise set default APP to SPMV (4)
if [ -z "$1" ]; then
    echo "Default APP is SPMV (4)"
    app=4
else
    app=$1 # [0: sssp, 1: pagerank, 2: bfs, 3: wcc, 4: spmv, 5: histogram, 6: fft, 7: spmv_alternative, 8: spmm]
fi

# Check if second argument is provided, otherwise set default test_case to 0
if [ -z "$2" ]; then
    test_case=0
else
    test_case=$2
fi

# Check if third argument is provided, otherwise set default name to "A"
if [ -z "$3" ]; then
    name="A"
else
    name=$3
fi

# Set default configurations
th=2 # Number of threads
assert_mode=1 # Enable assertions within the simulator code
verbose=2 # Level 1: print basic stats on each sample, Level 2: print full stats on each iteration, etc.
dry_run=0 # [0: run the simulator, 1: not running the simulator, only print the configuration block in the logs]

# Configuration for test_case 0
if [ $test_case -eq 0 ]; then
    datasets="Kron16"
    grid_x=16
    chiplet_w=$((grid_x))
    pack_w=$((grid_x))
    proxy_w=$((grid_x))
    sram_memory=$((256 * 512)) # 512 KiB
elif [ $test_case -eq 1 ]; then # Note that only the outputs of Kron16 are checked below
    datasets="Kron22"
    grid_x=64
    chiplet_w=16
    proxy_w=16
    pack_w=64
    sram_memory=$((256 * 1536)) # 1536 KiB
else
    echo "ERROR: Define your own configuration"
    exit 1
fi

#check if $datasets is equal to Kron16
if [ $datasets = "Kron16" ]; then
    set_conf "config_system" "sample_time" 1001
    local_run=2 # Run in the foreground (wait for the application result to check it with the reference)
else
    local_run=1 # Run in the background (not outputting the application functional result)
fi

# Dcache type (0: no dcache, only spd, 1: dcache if not fit on spd, 2: always dcache, 3: only prefetching and spatial locatity, 4: spatial locality only, 5: everything is a miss)
let dcache=1
let noc_type=1
let torus=1

params="-n ${name}${proxy_w} -o $torus -t $th -u $noc_type -v $verbose -s $local_run -e $proxy_w -c $chiplet_w -k $pack_w -r $assert_mode -w $sram_memory -y $dcache -p $dry_run -d $datasets -a $app -m $grid_x"
echo "Running $params"
exp/run.sh $params

# check output status
if [ $? -ne 0 ]; then
    echo "ERROR: run.sh failed"
    exit 1
fi

if [[ "$datasets" == "Kron16" ]]; then
    set_conf "config_system" "sample_time" 1000
    DIFF=""
    if [ $app -eq 0 ]; then
        echo "CHECKING OUTPUT..."
        DIFF=$(diff output.txt doall/sssp_K16_ref.txt)
    elif [ $app -eq 1 ]; then
        echo "CHECKED OUTPUT WITHIN APP!!!"
    elif [ $app -eq 2 ]; then
        echo "CHECKING OUTPUT..."
        DIFF=$(diff output.txt doall/bfs_K16_ref.txt)
    elif [ $app -eq 4 ] || [ $app -eq 7 ] || [ $app -eq 8 ]; then
        echo "CHECKING OUTPUT..."
        DIFF=$(diff output.txt doall/spmv_K16_ref.txt)
    elif [ $app -eq 5 ]; then
        echo "CHECKED OUTPUT WITHIN APP!!!"
    elif [ $app -eq 6 ]; then
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
    fi

    if [ "$DIFF" != "" ]; then
        echo "DIFF!!!"
        echo "$DIFF"
        exit 1
    fi
    echo "SUCCESS!!!"
fi