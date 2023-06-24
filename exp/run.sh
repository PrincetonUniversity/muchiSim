#!/bin/bash

Help(){ # Display Help
echo "Running Dalorex Simulation."
echo
echo "Syntax: run.sh [-n name | -a apps | -d datasets | -m mesh_conf | -o NOC | -b barrier | -x shuffle | -q queue_mode | -p sample power | -s machine_to_run | -t max_threads | -r assert_mode | -v verbose | -c chiplet_width | -e proxy_width | -k board_width | -w SRAM_space | -y dcache | -g large_queue | -i steal | -j cascading | -u noc_conf | -l ruche | -z proxy_routing]"
echo "options:"
echo "n     Name of the experiment."
echo "a     Application [0:SSSP; 1:PAGE; 2:BFS; 3:WCC; 4:SPMV]"
echo "d     Datasets to run [Kron16..., amazon-2003 ego-Twitter liveJournal wikipedia]"
echo "m     Mesh size [Powers of 2]"
echo "o     Noc type [0:mesh, 1:torus, 2:3d-mesh]"
echo "b     Barrier Mode [0:no barrier, 1:global barrier, 2:tesseract barrier, 3 barrier no coalescing]"
echo "x     Shuffle Dataset [Default 1]"
echo "q     Queue priority mode [Default 0:dalorex, 1:time-sorted, 2:round-robin]"
echo "p     Sampling time, in Powers of 10 [Default: 5 -> (10^5)]"
echo "s     If running on the local machine, set to 1. If running on a slurm local_run, set to 0."
echo "t     Max number of threads"
echo "r     Assert mode on: activates the runtime assertion checks (lower speed), only run this if suspecting programming error. This mode also has more debug output"
echo "v     Verbose mode: If activated (1), then it prints the current thread status (processed edges, nodes and idle time) at each sample time. A value of (2) gives inter router queue output too."
echo "c     Chiplet width"
echo "e     Replica width"
echo "k     Board width"
echo "w     SRAM space"
echo "y     Configuration mode of data cache and prefetching"
echo "g     Large queue"
echo "i     Steal option"
echo "j     Cascading option"
echo "u     NOC configuration"
echo "l     Ruche option"
echo "z     Proxy routing"
echo
   
}

############################################################
# Main program                                             #
############################################################
function log2 {
    local x=0
    for (( y=$1-1 ; $y > 0; y >>= 1 )) ; do
        let x=$x+1
    done
    echo $x
}

bin="FF"

# Default no print debug, results in folder runs, max number of emulation threads 16
verbose=0
assert_mode=0
folder="dlxsim"
sbatch_folder="dlxsim_sbatch"
max_threads=16
local_run=0

# Default Torus, no ruche, no barrier, and Scratchpad as big as dataset!
torus=1

#Ruche channel length, 99 is do not set it based on this option, but whatever is in macros.h by default
ruche=99

# NOC experiment type.
# NOC_CONF:0 means 2 NoCS of 32 bits intra die, which get throttled to a shared 32 bits across dies
# NOC_CONF:1 means 2 NoCS of 32 and 64 bit intra die, which get throttled to a shared 32 bits across dies
# NOC_CONF:2 means 2 NoCS of 32 and 64 bit intra die, which get throttled to 2 NoCs of 32 and 32 bits across dies
# NOC_CONF:3 means 2 NoCS of 32 and 64 bit intra and inter-die
noc_conf=2

# How many iterations a task can run before parking
loop_chunk=64

#Deprecated (only option 0 available, TSU is always on, no RR)
queue_prio=0

# Power of 2 of the sample time
powers_sample_time=0
proxy_w=0
chiplet_w=0
board_w=0
sram_memory=256*4096 # 4096 KiB
dcache=0

# Default SPMV on 8x8 and Kron16
apps="4"
m="8"
datasets="Kron16"

# Default shuffle (randomize dataset)
shuffle=0

#On-the-way&cascading (11)? or within&no-cascading (00)
#Cascading write-backs from proxys to the owner (0: direct-to-owner, 1: cascading)
cascading=0
# Within Proxy Region (0) or on-the-way (1), selective (4)
proxy_routing=4

# Queue size experiments (0: default, 1+ experiment increases sizes)
large_queue=0

# Steal regions (0: no steal, 1+: steal region size, pow2)
steal=0

############################################################
# Process the input options.                               #
############################################################
while getopts ":hs:n:a:d:m:b:t:r:o:x:p:q:v:f:e:c:k:w:y:g:i:j:u:l:z:" flag
do
  case "${flag}" in
    a) apps=${OPTARG};;
    b) barrier=${OPTARG};;
    c) chiplet_w=${OPTARG};;
    d) datasets=${OPTARG};;
    e) proxy_w=${OPTARG};;
    f) folder=${OPTARG};;
    g) large_queue=${OPTARG};;
    h) Help; exit;;
    i) steal=${OPTARG};;
    j) cascading=${OPTARG};;
    k) board_w=${OPTARG};;
    l) ruche=${OPTARG};;
    m) m=${OPTARG};;
    n) bin=${OPTARG};;
    o) torus=${OPTARG};;
    p) powers_sample_time=${OPTARG};;
    q) queue_prio=${OPTARG};;
    r) assert_mode=${OPTARG};;
    s) local_run=${OPTARG};;
    t) max_threads=${OPTARG};;
    u) noc_conf=${OPTARG};;
    v) verbose=${OPTARG};;
    w) sram_memory=${OPTARG};;
    x) shuffle=${OPTARG};;
    y) dcache=${OPTARG};;
    z) proxy_routing=${OPTARG};;
    \?) echo "Error: Invalid option"; exit;;
  esac
done

let log2m=$(log2 $m)

if [ $powers_sample_time -eq 0 ]; then
    if [ $log2m -lt 3 ]; then
        powers_sample_time=7
    elif [ $log2m -lt 4 ]; then
        powers_sample_time=6
    elif [ $log2m -lt 6 ]; then
        powers_sample_time=5
    elif [ $log2m -lt 8 ]; then
        powers_sample_time=4
    elif [ $log2m -lt 10 ]; then
        powers_sample_time=3
    elif [ $log2m -lt 12 ]; then
        powers_sample_time=2
    fi
fi

let sample_time="10**$powers_sample_time"

run(){
  echo \#APP=$a
  echo \#DATASET=$d
  echo \#MESH=$m-x-$m
  echo \#SIM_THREADS=$cores

  d_sub=${d:0:1}
  d_sub2=${d:4:6}
  bin_name="${bin}a${a}m${m}-${d_sub}${d_sub2}.run"
  echo $bin_name

  let barrier_post=0
  binary="bin/$bin_name"
  # If $a is 1 or 2, then we are running a barrier app
  if [ $a -eq 0 ] || [ $a -eq 1 ] || [ $a -eq 6 ]; then
    let barrier_post=1
  else
    let barrier_post=0
  fi

  
  if [ $steal -eq 1 ]; then
    let steal_w=4
  else
    let steal_w=1
  fi

  #write an assertion that checks if $cascading is 1 and $proxy_routing is 4
  if [ $cascading -eq 1 ] && [ $proxy_routing -eq 4 ]; then
   echo "Disabled mode"
   exit
  fi
  let chiplet_h=$chiplet_w

  options="-DSHUFFLE=$shuffle -DQUEUE_PRIO=$queue_prio -DMAX_THREADS=$cores -DASSERT_MODE=$assert_mode -DPRINT=$verbose -DSAMPLE_TIME=$sample_time -DDCACHE=$dcache -DSRAM_SIZE=$sram_memory -DGLOBAL_BARRIER=$barrier_post -DTORUS=$torus -DGRID_X_LOG2=$log2m -DAPP=$a -DDIE_W=$chiplet_w -DDIE_H=$chiplet_h -DBOARD_W=$m -DPROXY_W=$proxy_w -DTEST_QUEUES=$large_queue -DCASCADE_WRITEBACK=$cascading -DSTEAL_W=$steal_w -DNOC_CONF=$noc_conf -DLOOP_CHUNK=$loop_chunk -DPROXY_ROUTING=$proxy_routing"

  if [ $ruche -lt 99 ]
  then
    options="$options -DRUCHE_X=$ruche"
  fi

  case "$PWD" in
    *"/Users/movera/git/"*)
      compiler="g++-13"
      ;;
    *)
      compiler="g++"
      ;;
  esac

  compile="$compiler src/main.cpp -fopenmp -o $binary $options -O3"
  echo $compile
  $compile
  output_file="DATA-$d--$m-X-$m--B$bin-A$a.log"
  result_file="RES-$d--$m-X-$m--B$bin-A$a.log"
  echo $output_file
  echo $result_file

  dataset_folder="datasets"

  # check if local_run has values bigger than 0
  if [ $local_run -gt 0 ]; then
    pwd
    cmd="$binary $dataset_folder/$d/ > $folder/$output_file"
    echo $cmd
    echo
    #gdb $binary
    #check if local_run is 1 or 2
    if [ $local_run -eq 1 ]; then
      #numactl --cpubind 5 
      bin/$bin_name $dataset_folder/$d/ > $folder/DATA-$d--$m-X-$m--B$bin-A$a.log &
    else
      bin/$bin_name $dataset_folder/$d/ output.txt > $folder/DATA-$d--$m-X-$m--B$bin-A$a.log
    fi
  else #della

    let hours=24
    if [ $m -gt 256 ]; then
      let hours=96
    elif [ $m -gt 64 ]; then
      let hours=48
    fi

    slurm_file="SLURM-$d--$m-X-$m--B$bin-A$a.log"
    echo "#!/bin/bash"                        > batch.sh
    echo "#SBATCH --time=$hours:00:00"           >> batch.sh
    echo "#SBATCH --output=$sbatch_folder/$slurm_file" >> batch.sh
    echo "#SBATCH --mem-per-cpu=8G" >> batch.sh
    echo "#SBATCH --cpus-per-task=$cores"   >> batch.sh
    echo "#SBATCH --job-name=A$a-G$m-$d-B$bin" >> batch.sh
    # echo "#SBATCH --gres=gpu:a100:0"        >> batch.sh
    #echo "#SBATCH --mail-type=begin"       >> batch.sh        # send email when job begins
    # echo "#SBATCH --mail-type=end"          >> batch.sh        # send email when job ends
    # echo "#SBATCH --mail-user=movera@princeton.edu" >> batch.sh
    echo "module load gcc-toolset/10"       >> batch.sh
    cmd="srun ./$binary $PWD/$dataset_folder/$d/ > $folder/$output_file"
    echo $cmd; echo $cmd >> batch.sh; sbatch batch.sh;
    # squeue -u movera --format="%.18i %.9P %.50j %.8u %.2t %.10M %.6D %R"
  fi
}

if [ $proxy_w -eq 0 ]
then
  proxy_w=$m
fi

# Determine number of threads to use
if [[ $m -gt $max_threads ]]
then
  cores=$max_threads
else
  cores=$m
fi

for d in $datasets
do
  for a in $apps
  do
    run
  done
done

