#!/bin/bash
if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
    echo "Error: Missing required parameters."
    exit 1
fi

if [ $1 -gt 5 ]; then
  apps="0 1 2 3 4 5"
else
  apps=$1
fi

if [ -z "$4" ]; then
  echo "All datasets by default"
  datasets="Kron22 wikipedia"
else
  echo "Dataset $4"
  datasets="$4"
fi

declare -A options
declare -A strings

th=16
verbose=1
assert=0
iter="MEM"

let grid_w=32

i=0
let noc_conf=1
let dcache=1

local_run=0

# Torus
let torus=1
let board_w=$grid_w #Specify board so that the package has the same size as the board
sufix="-v $verbose -r $assert -t $th -u $noc_conf -m $grid_w -k $board_w -o $torus -y $dcache -s $local_run"

# Grid of 32x32, Torus.
# 32x32 at 64KB, 128KB, 256KB; 16x16 at 128KB, 256KB, 512KB.

let chiplet_w=32
#C0
let sram_memory=256*64
strings[$i]="${iter}0"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

#C1
let sram_memory=256*128
strings[$i]="${iter}1"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

#C2
let sram_memory=256*256
strings[$i]="${iter}2"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

let chiplet_w=16
#C3
let sram_memory=256*128
strings[$i]="${iter}3"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

#C4
let sram_memory=256*256
strings[$i]="${iter}4"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

#C5
let sram_memory=256*512
strings[$i]="${iter}5"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

#C6
let chiplet_w=32
strings[$i]="${iter}6"
options[$i]="-n ${strings[$i]} -c $chiplet_w -w $sram_memory $sufix"
let i=$i+1

###############

for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done
