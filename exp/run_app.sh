#!/bin/bash

#make sure $1 is set
if [ -z "$1" ]; then
    echo "ERROR: \$1 is not set"
    exit 1
fi
app=$1
th=4

assert_mode=1
# Level1: print basic stats on each sample, Level2: print full stats on each iteration, Level3: print queue stats, Level 4: print full debug stats
verbose=1

test_dcache=1
if [ $2 -eq 0 ]; then
    datasets="Kron16"
    sampling=3
    if [ $test_dcache -eq 1 ]; then
        configs="8"
        let chiplet_w=4
        let pack_w=16
        let proxy_w=4
        let sram_memory=256*1024 # 1024 KiB
    else
        configs="16"
        let chiplet_w=16
        let pack_w=16
        let proxy_w=4
        let sram_memory=256*512 # 512 KiB
    fi
elif [ $2 -eq 1 ]; then
    datasets="Kron22"
    sampling=5
    configs="64"
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


let ruche=$chiplet_w/2
let large_queue=0
# Dcache type (0: no dcache, only spd, 1: dcache if not fit on spd, 2: always dcache, 3: only prefetching and spatial locatity, 4: spatial locality only, 5: everything is a miss)
let dcache=0
let barrier=0
let shuffle=0

let cascade=0
let routing=4

let noc_type=2

cmd="-n ${name}${proxy_w} -o 1 -t $th -b $barrier -u $noc_type -x $shuffle -v $verbose -s $local_run -e $proxy_w -c $chiplet_w -k $pack_w -r $assert_mode -w $sram_memory -y $dcache -g $large_queue -l $ruche -p $sampling -z $routing -j $cascade"
exp/run.sh $cmd -d "$datasets" -a "$app" -m "$configs"
# check output status
if [ $? -ne 0 ]; then
    echo "ERROR: run.sh failed"
    exit 1
fi

if [ $datasets = "Kron16" ]; then
    if [ $app -eq 0 ]; then
        DIFF=$(diff output.txt doall/sssp_K16_ref.txt)
    elif [ $app -eq 1 ]; then
        DIFF=""
    elif [ $app -eq 2 ]; then
        DIFF=$(diff output.txt doall/bfs_K16_ref.txt)
    elif [ $app -eq 3 ]; then
        DIFF=$(diff output.txt doall/wcc_K16B_ref.txt)
    elif [ $app -eq 4 ]; then
        DIFF=$(diff output.txt doall/spmv_K16B_ref.txt)
    elif [ $app -eq 5 ]; then
       DIFF=""
    fi
    if [ "$DIFF" != "" ]; then
        echo "DIFF!!!"
        echo "$DIFF"
        exit 1
    fi
    echo "SUCCESS!!!"
fi