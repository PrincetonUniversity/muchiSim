#!/bin/bash

Help(){ # Display Help
echo "Running Dalorex Simulation."
echo
echo "Syntax: run.sh [-n name | -a apps | -d datasets | -m grid_conf | -o NOC | -b barrier | -x num_phy_nocs | -q queue_mode | -p dry_run | -s machine_to_run | -t max_threads | -r assert_mode | -v verbose | -c chiplet_width | -e proxy_width | -k board_width | -w SRAM_space | -y dcache | -g large_queue | -i steal | -j write_thru | -u noc_conf | -l ruche | -z proxy_routing]"
echo "options:"
echo "n     Name of the experiment."
echo "a     Application [0:SSSP; 1:PAGE; 2:BFS; 3:WCC; 4:SPMV]"
echo "d     Datasets to run [Kron16..., amazon-2003 ego-Twitter liveJournal wikipedia]"
echo "m     Grid size [Powers of 2]"
echo "o     Noc type [0:mesh, 1:torus]"
echo "b     Barrier Mode [0:no barrier, 1:global barrier, 2:tesseract barrier, 3 barrier no coalescing]"
echo "x     Num Physical NoCs"
echo "q     Queue priority mode [Default 0:dalorex, 1:time-sorted, 2:round-robin]"
echo "p     Dry run (no simulation, only print configuration) [Default 0]"
echo "s     If running on the local machine, set to 1. If running on a slurm local_run, set to 0."
echo "t     Max number of threads"
echo "r     Assert mode on: activates the runtime assertion checks (lower speed), only run this if suspecting programming error. This mode also has more debug output"
echo "v     Verbose mode: If activated (1), then it prints the current thread status (processed edges, nodes and idle time) at each sample time. A value of (2) gives inter router queue output too."
echo "c     Chiplet width"
echo "e     Proxy width"
echo "k     Board width"
echo "w     SRAM space"
echo "y     Configuration mode of data cache and prefetching"
echo "g     Large queue"
echo "i     Steal option"
echo "j     write_thru"
echo "u     NOC configuration"
echo "l     Ruche option"
echo "z     Proxy routing"
echo "f     Force proxy ratio"
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
folder="sim_logs"
sbatch_folder="sim_sbatch"
max_threads=16
local_run=0

# Default Torus, no ruche
let torus=1

#Ruche channel length, 99 is do not set it based on this option, but whatever is in macros.h by default
ruche=99

# NOC experiment type.
# NOC_CONF:0 means 2 NoCS of 32 bits intra die, which get throttled to a shared 32 bits across dies
# NOC_CONF:1 means 2 NoCS of 32 and 64 bit intra die, which get throttled to a shared 32 bits across dies
# NOC_CONF:2 means 2 NoCS of 32 and 64 bit intra die, which get throttled to 2 NoCs of 32 and 32 bits across dies
# NOC_CONF:3 means 2 NoCS of 32 and 64 bit intra and inter-die
let noc_conf=2
let phy_nocs=1 #default 1

# How many iterations a task can run before parking
let loop_chunk=64

#Deprecated (only option 0 available, TSU is always on, no RR)
let queue_prio=0

# Power of 2 of the sample time
let powers_sample_time=0
let dry_run=0
let proxy_w=0
let chiplet_w=0
let board_w=0
let sram_memory=256*512 # 512 KiB
let dcache=0
let force_proxy_ratio=0

# Default SPMV on 8x8 and Kron16
grid_x=8

# Functional simulation
functional=1

# 1: On-the-wayCascading,  3: Within+NoCascading, 4:Within+SelectiveCascading, 5:Within+AllCascading
proxy_routing=4
# Set default based on App (below)
write_thru=99
barrier=99

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
    f) force_proxy_ratio=${OPTARG};;
    g) large_queue=${OPTARG};;
    h) Help; exit;;
    i) steal=${OPTARG};;
    j) write_thru=${OPTARG};;
    k) board_w=${OPTARG};;
    l) ruche=${OPTARG};;
    m) grid_x=${OPTARG};;
    n) bin=${OPTARG};;
    o) torus=${OPTARG};;
    p) dry_run=${OPTARG};;
    q) queue_prio=${OPTARG};;
    r) assert_mode=${OPTARG};;
    s) local_run=${OPTARG};;
    t) max_threads=${OPTARG};;
    u) noc_conf=${OPTARG};;
    v) verbose=${OPTARG};;
    w) sram_memory=${OPTARG};;
    x) phy_nocs=${OPTARG};;
    y) dcache=${OPTARG};;
    z) proxy_routing=${OPTARG};;
    \?) echo "Error: Invalid option"; exit;;
  esac
done

let log2grid_x=$(log2 $grid_x)




run_batch(){
  echo \#APP=$a
  echo \#DATASET=$d
  echo \#MESH=$grid_x-x-$grid_x
  echo \#SIM_THREADS=$cores

  d_sub=${d:0:1}
  d_sub2=${d:4:6}
  bin_name="${bin}a${a}g${grid_x}-${d_sub}${d_sub2}.run"
  echo $bin_name

  barrier_post=$barrier
  binary="bin/$bin_name"
  if [ $barrier_post -eq 99 ]; then # If barrier not defined in the options, set it based on the app
    if [ $a -eq 1 ] || [ $a -eq 10 ]; then
      barrier_post=1
    else
      barrier_post=0
    fi
  fi


  write_thru_post=$write_thru
  # 1: Write Thru, 0: Write Back
  if [ $write_thru_post -eq 99 ]; then # Not set in the options, default value for apps
    if [ $a -eq 1 ] || [ $a -eq 4 ] || [ $a -eq 5 ] || [ $a -eq 7 ]; then #Apps without frontier or with barrier
      write_thru_post=0
    else # 0 2 3
      write_thru_post=1
    fi
  fi

  if [ $steal -eq 1 ]; then
    let steal_w=4
  else
    let steal_w=1
  fi

  # Default value for $chiplet_w is $grid_x
  if [ $chiplet_w -eq 0 ]; then
    chiplet_w=$grid_x
  fi
  let chiplet_h=$chiplet_w

  # Default value for $proxy_w is $grid_x
  if [ $proxy_w -eq 0 ]; then
    proxy_w=$grid_x
  fi

  # If no proxy, we make sure the proxy routing is off
  if [ $proxy_w -eq $grid_x ]; then 
    proxy_routing=0
  fi

  # Determine number of threads to use
  if [[ $grid_x -gt $max_threads ]]
  then
    cores=$max_threads
  else
    cores=$grid_x
  fi

  options=""
  if [ $phy_nocs -gt 1 ]; then
    options="-DPHY_NOCS=$phy_nocs"
  fi
  # Macros not actively used: -DSTEAL_W=$steal_w -DSHUFFLE=$shuffle -DQUEUE_PRIO=$queue_prio 

  options="-DMAX_THREADS=$cores -DASSERT_MODE=$assert_mode -DPRINT=$verbose -DDCACHE=$dcache -DSRAM_SIZE=$sram_memory -DGLOBAL_BARRIER=$barrier_post -DTORUS=$torus -DGRID_X_LOG2=$log2grid_x -DAPP=$a -DDIE_W=$chiplet_w -DDIE_H=$chiplet_h -DPROXY_W=$proxy_w -DTEST_QUEUES=$large_queue -DWRITE_THROUGH=$write_thru_post -DNOC_CONF=$noc_conf -DLOOP_CHUNK=$loop_chunk -DPROXY_ROUTING=$proxy_routing -DFUNC=$functional $options"
  
  if [ $force_proxy_ratio -gt 0 ]; then
    options="$options -DFORCE_PROXY_RATIO=$force_proxy_ratio"
  fi

  # Only set ruche if it is not the default value
  if [ $ruche -lt 99 ]; then
    options="$options -DRUCHE_X=$ruche"
  fi

  # Default value for $board_w is $grid_x
  if [ $board_w -gt 0 ]; then
    options="$options -DBOARD_W=$board_w"
  fi


  case "$PWD" in
    *"/Users/movera/git/"*)
      #compiler="g++-13"
      compiler="/usr/local/opt/llvm/bin/clang++"
      ;;
    *)
      compiler="g++"
      ;;
  esac

  compile="$compiler src/main.cpp -fopenmp -o $binary $options -O3"
  echo $compile
  $compile
  output_file="DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
  result_file="RES-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
  echo $output_file
  echo $result_file

  dataset_folder="datasets"

  # check if local_run > 0
  if [ $local_run -gt 2 ]; then
    echo "Error: local_run must be 0, 1 or 2"
  elif [ $local_run -gt 0 ]; then
    cmd="$binary $dataset_folder/$d/ > $folder/$output_file"
    echo $cmd; echo
    #check if local_run is 1 or 2
    if [ $local_run -eq 1 ]; then
      #numactl --cpubind 5 
      bin/$bin_name $dataset_folder/$d/ $bin $dry_run > $folder/DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log &
    else
      bin/$bin_name $dataset_folder/$d/ $bin $dry_run output.txt > $folder/DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log
    fi
  else #della (local_run=0)
    let hours=24
    if [ $grid_x -gt 256 ]; then
      let hours=96
    elif [ $grid_x -gt 64 ]; then
      let hours=48
    fi
    slurm_file="SLURM-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
    echo "#!/bin/bash"                        > batch.sh
    echo "#SBATCH --time=$hours:00:00"           >> batch.sh
    echo "#SBATCH --output=$sbatch_folder/$slurm_file" >> batch.sh
    echo "#SBATCH --mem-per-cpu=8G" >> batch.sh
    echo "#SBATCH --cpus-per-task=$cores"   >> batch.sh
    echo "#SBATCH --job-name=A$a-G$grid_x-$d-B$bin" >> batch.sh
    echo "#SBATCH --gres=gpu:a100:0"        >> batch.sh
    #echo "#SBATCH --mail-type=begin"       >> batch.sh        # send email when job begins
    # echo "#SBATCH --mail-type=end"          >> batch.sh        # send email when job ends
    # echo "#SBATCH --mail-user=movera@princeton.edu" >> batch.sh
    echo "module load gcc-toolset/10"       >> batch.sh
    cmd="srun ./$binary $PWD/$dataset_folder/$d/ $bin $dry_run > $folder/$output_file"
    echo $cmd; echo $cmd >> batch.sh; sbatch batch.sh;
    squeue -u movera --format="%.18i %.9P %.50j %.8u %.2t %.10M %.6D %R"
  fi
}

echo "apps=$apps"
echo "datasets=$datasets"

for d in $datasets
do
  for a in $apps
  do
    echo "RUNNING $a $d"
    run_batch
  done
done

