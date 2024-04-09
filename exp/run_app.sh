#!/bin/bash
if [ -z "$1" ]; then
    echo "ERROR: \$1 is not set"
    exit 1
fi
app=$1
th=4

assert_mode=1
# Level1: print basic stats on each sample, Level2: print full stats on each iteration, Level3: print queue stats, Level 4: print full debug stats
verbose=1

let test_case=$2
if [ -z "$2" ]; then
    test_case=0
fi
if [ $test_case -eq 0 ]; then
    datasets="Kron16"
    grid_x=64
    let chiplet_w=$grid_x
    let pack_w=$grid_x
    let proxy_w=$grid_x
    let sram_memory=256*512 # 512 KiB
elif [ $test_case -eq 1 ]; then # Note that only the outputs of Kron16 are checked below
    datasets="Kron22"
    grid_x=64
    let chiplet_w=16
    let proxy_w=16
    let pack_w=64
    let sram_memory=256*1536 # 1536 KiB
else
    echo "ERROR: Define your own configuration"
    exit 1
fi

if [ -z $3 ]; then
    name="A"
else
    name=$3
fi
#check if $datasets is equal to Kron16
if [ $datasets = "Kron16" ]; then
    local_run=2  # Run in the foreground (wait for the output)
else
    local_run=1 # Run in the background
fi


# Dcache type (0: no dcache, only spd, 1: dcache if not fit on spd, 2: always dcache, 3: only prefetching and spatial locatity, 4: spatial locality only, 5: everything is a miss)
let dcache=1
let noc_type=1
let torus=1
let dry_run=0

cmd="-n ${name}${proxy_w} -o $torus -t $th -u $noc_type -v $verbose -s $local_run -e $proxy_w -c $chiplet_w -k $pack_w -r $assert_mode -w $sram_memory -y $dcache -p $dry_run"

exp/run.sh $cmd -d "$datasets" -a "$app" -m $grid_x
# check output status
if [ $? -ne 0 ]; then
    echo "ERROR: run.sh failed"
    exit 1
fi

if [ $datasets = "Kron16" ]; then
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