#!/bin/bash
if [ -z "$MUCHI_ROOT" ]; then
    echo "MUCHI_ROOT is not set, run: source setup.sh"
    exit 1
fi
source $MUCHI_ROOT/exp/util.sh;

if [ $1 -gt 5 ]; then
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

th=2
verbose=2 # Verbosity level needed for heatmaps
assert=0
exp="HEAT"

if [[ "$datasets" == "Kron16" ]]; then
  set_conf "config_system" "sample_time" 1001
fi

let noc_conf=1
let dcache=0
let ruche=0
let torus=0

let chiplet_w=$grid_w
let board_w=$grid_w #Specify board so that the package has the same size as the board
 
sufix="-v $verbose -r $assert -t $th -u $noc_conf -m $grid_w -c $chiplet_w -k $board_w -l $ruche -y $dcache -s $LOCAL_RUN"

i=0
strings=()
options=()

# MESH without Tascade Reductions, with Barrier
strings[$i]="${exp}0"
options[$i]="-n ${strings[$i]} -e $grid_w -o 0 -b 1 $sufix"
let i=$i+1

# TORUS without Tascade Reductions, with Barrier
strings[$i]="${exp}1"
options[$i]="-n ${strings[$i]} -e $grid_w -o 1 -b 1 $sufix"
let i=$i+1

# TORUS with Tascade Reductions, with Barrier
strings[$i]="${exp}2"
options[$i]="-n ${strings[$i]} -e 8 -o 1 -j 0 -b 1 $sufix"
let i=$i+1

#####
# MESH with Tascade Reductions, without Barrier
strings[$i]="${exp}8M"
options[$i]="-n ${strings[$i]} -e 8 -o 0 $sufix"
let i=$i+1

# MESH without Tascade Reductions, without Barrier
strings[$i]="${exp}64M"
options[$i]="-n ${strings[$i]} -e 64 -o 0 $sufix"
let i=$i+1


###############

for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done

set_conf "config_system" "sample_time" 1000
