#!/bin/bash

if [ $1 -gt 5 ]; then
  apps="0 1 2 3 4 5"
else
  apps=$1
fi

#datasets="Kron22 wikipedia"
datasets="wikipedia"
declare -A options
declare -A strings

th=16
verbose=1
assert=0
exp="PROXY"

i=0
let sram_memory=256*1536

let mesh_w=64
let chiplet_w=16

let proxy_routing=4
let cascading=0

let noc_conf=2
let shuffle=0
let dcache=1

# Run on Della:0 (need to be logged into della), run on Chai:1
local_run=0

sufix="-v $verbose -r $assert -t $th -u $noc_conf -m $mesh_w -c $chiplet_w -w $sram_memory -z $proxy_routing -j $cascading -x $shuffle -y $dcache -s $local_run"

let proxy_w=$mesh_w/8
# P == MESH/8
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w $sufix"
let i=$i+1

# P == MESH/4
let proxy_w=$proxy_w*2
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1

# P == MESH/2
let proxy_w=$proxy_w*2
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1

# P == MESH
let proxy_w=$proxy_w*2
strings[$i]="${exp}${proxy_w}"
options[$i]="-n ${strings[$i]} -e $proxy_w  $sufix"
let i=$i+1


###############

for c in $(seq $2 $3) #inclusive
do
  echo ${strings[$c]}
  echo "${options[$c]}"
  echo
  exp/run.sh ${options[$c]} -d "$datasets" -a "$apps"
done
