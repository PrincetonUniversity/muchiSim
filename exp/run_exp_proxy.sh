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
  let grid_w=128
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

declare -A options
declare -A strings

th=8
verbose=1
assert=0
exp="PROXY"

# Monolithic SRAM, runs 128x128, with varying proxy sizes
let noc_conf=1
let dcache=0
let ruche=0
let torus=1

let chiplet_w=$grid_w
let board_w=$grid_w #Specify board so that the package has the same size as the board

sufix="-v $verbose -s $LOCAL_RUN -r $assert -t $th -u $noc_conf -m $grid_w -c $chiplet_w -k $board_w -l $ruche -o $torus -y $dcache"
i=0

# No proxy
let proxy_w=$grid_w
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1

# Proxy 32x32
let proxy_w=32
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w $sufix"
let i=$i+1

let proxy_w=16
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1

let proxy_w=8
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1

#### Fraction of the proxy cache size
let proxy_w=16
strings[$i]="${exp}${proxy_w}F"
options[$i]="-n ${strings[$i]} -e $proxy_w -f 4 $sufix"
let i=$i+1

let proxy_w=8
strings[$i]="${exp}${proxy_w}F"
options[$i]="-n ${strings[$i]} -e $proxy_w -f 16 $sufix"
let i=$i+1


###############

for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done
