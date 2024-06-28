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

datasets="Kron26"

verbose=1
assert=0
exp="SCALINGD"

let noc_conf=1
let dcache=1

let grid_w=32
let chiplet_w=32
let proxy_w=16
if [ $1 -eq 6 ]; then
  let proxy_w=0
fi

prefix="-v $verbose -r $assert -u $noc_conf -c $chiplet_w -y $dcache -s $LOCAL_RUN"

i=0
declare -A options
declare -A strings

####### START EXPERIMENT #######
#0 - 32
let th=$grid_w/2
strings[$i]="000 Min size"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1


#1 - 64
let grid_w=$grid_w*2
let th=$grid_w/2
strings[$i]="111 Min size"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1

#2 - 128
let grid_w=$grid_w*2
let th=32
strings[$i]="222 Scaling step"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1


# De-activate the cache for more than 128x128 (Kron26)
dcache=0
prefix="-v $verbose -r $assert -u $noc_conf -c $chiplet_w -y $dcache -s $LOCAL_RUN"

#3 - 256
let th=32
let grid_w=$grid_w*2
strings[$i]="333 Scaling step"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1


# Big cases OOM with Proxy16
let proxy_w=32
if [ $1 -eq 6 ]; then
  let proxy_w=0
fi
#4 - 512
let th=32
let grid_w=$grid_w*2
strings[$i]="444 Scaling step"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1

#5: 1024
let th=64
let proxy_w=32
let grid_w=$grid_w*2
strings[$i]="555 Scaling step"
options[$i]="-n ${exp} -e $proxy_w -m $grid_w -t $th $prefix"
let i=$i+1



for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done
