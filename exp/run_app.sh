#!/bin/bash

#make sure $1 is set
if [ -z "$1" ]; then
    echo "ERROR: \$1 is not set"
    exit 1
fi
app=$1
th=8

assert_mode=1
# Level1: print basic stats on each sample, Level2: print full stats on each iteration, Level3: print queue stats, Level 4: print full debug stats
verbose=1

test=2
if [ $2 -eq 0 ]; then
    datasets="Kron16"
    if [ $test -eq 1 ]; then
        grid_x=8
        let chiplet_w=8
        let pack_w=8
        let proxy_w=8
        let sram_memory=256*128 # 1024 KiB
    else
        grid_x=16
        let chiplet_w=16
        let pack_w=16
        let proxy_w=16
        let sram_memory=256*512 # 512 KiB
    fi
elif [ $2 -eq 1 ]; then
    datasets="Kron22"
    grid_x=64
    let chiplet_w=16
    let proxy_w=16
    let pack_w=64
    let sram_memory=256*1536 # 1536 KiB
fi

if [ -z $3 ]; then
    name="E"
else
    name=$3
fi
#check if $datasets is equal to Kron16
if [ $datasets = "Kron16" ]; then
    local_run=2
else
    local_run=1
fi


let large_queue=0
# Dcache type (0: no dcache, only spd, 1: dcache if not fit on spd, 2: always dcache, 3: only prefetching and spatial locatity, 4: spatial locality only, 5: everything is a miss)
let dcache=0
let routing=4
let write_thru=0

let noc_type=1
let torus=0
let dry_run=0

cmd="-n ${name}${proxy_w} -o $torus -t $th -u $noc_type -v $verbose -s $local_run -e $proxy_w -c $chiplet_w -k $pack_w -r $assert_mode -w $sram_memory -y $dcache -g $large_queue -p $dry_run -z $routing" # -j $write_thru"

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
    elif [ $app -eq 3 ]; then
        echo "CHECKING OUTPUT..."
        DIFF=$(diff output.txt doall/wcc_K16B_ref.txt)
    elif [ $app -eq 4 ] || [ $app -eq 7 ]; then
        echo "CHECKING OUTPUT..."
        DIFF=$(diff output.txt doall/spmv_K16B_ref.txt)
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