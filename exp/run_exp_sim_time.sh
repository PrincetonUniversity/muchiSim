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
  echo "Default grid_w=64"
  let grid_w=64
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

declare -A options
declare -A strings

let verbose=0
let assert=0

exp="ITHR"

i=0
let sram_memory=256*512

# Monolithic
let chiplet_w=$grid_w
let noc_conf=1
let dcache=1


sufix="-v $verbose -r $assert -u $noc_conf -m $grid_w -c $chiplet_w -w $sram_memory -y $dcache -s $LOCAL_RUN"

#0 - 2th
let th=2
strings[$i]="${exp}${th}"
options[$i]="-n ${strings[$i]} -t $th $sufix"
let i=$i+1

#1 - 4th
let th=$th*2
strings[$i]="${exp}${th}"
options[$i]="-n ${strings[$i]} -t $th  $sufix"
let i=$i+1

#2 - 8th
let th=$th*2
strings[$i]="${exp}${th}"
options[$i]="-n ${strings[$i]} -t $th  $sufix"
let i=$i+1

#3 - 16th
let th=$th*2
strings[$i]="${exp}${th}"
options[$i]="-n ${strings[$i]} -t $th  $sufix"
let i=$i+1

#4 - 32th
let th=$th*2
strings[$i]="${exp}${th}"
options[$i]="-n ${strings[$i]} -t $th  $sufix"
let i=$i+1

###############

for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done
