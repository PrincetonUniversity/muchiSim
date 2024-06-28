#!/bin/bash

# DISPLAY HELP MESSAGE
Help(){
echo "MuchiSim Run Script."
echo
echo "Syntax: run.sh [-a apps | -b barrier | -c chiplet_width | -d datasets | -e proxy_width | -f force_proxy_ratio | -g large_queue | -j write_thru | -k board_width | -l express_link | -m grid_conf | -n name | -o NOC | -p dry_run | -q queue_mode | -r assert_mode | -s machine_to_run | -t max_threads | -u noc_conf | -v verbose | -w SRAM-per-tile (KiBs) | -x num_phy_nocs | -y dcache | -z proxy_routing]"
echo "options:"
echo "a     Application [0:SSSP; 1:PAGE; 2:BFS; 3:WCC; 4:SPMV, 5:Histogram, 6: FFT, 8:SPMM]"
echo "b     Barrier Mode [0:no barrier, 1:global barrier, 2:barrier merge reduction tree]"
echo "c     Chiplet width"
echo "d     Datasets to run [Kron16..., amazon-2003 ego-Twitter liveJournal wikipedia]"
echo "e     Proxy width"
echo "f     Force proxy ratio"
echo "g     Large queue"
echo "j     Write-propagation [0:write-back, 1:write-thru, default: based on the app]"
echo "k     Node Board width [Default: same as grid size]"
echo "l     Express link skip-tile for monolithic, or inter-die channels for multi-chiplet systems"
echo "m     Grid size [Powers of 2]"
echo "n     Name of the experiment."
echo "o     NoC type [0:mesh, 1:torus]"
echo "p     Dry run (no simulation, only print configuration) [Default 0]"
echo "q     Queue priority mode [Default 0:dalorex, 1:time-sorted, 2:round-robin]"
echo "r     Assert mode on: activates the runtime assertion checks (lower speed), only run this if suspecting programming error. This mode also has more debug output"
echo "s     If running on the local machine, set to 1. If running on SLURM set local_run=0."
echo "t     Max number of threads"
echo "u     NoC width configuration [0, 1, 2, 3] (see run.sh for details)"
echo "v     Verbose mode: If activated (1), then it prints the current thread status (processed edges, nodes and idle time) at each sample time. A value of (2) gives inter router queue output too."
echo "w     SRAM-per-tile (KiBs)"
echo "x     Num Physical NoCs"
echo "y     Configuration mode of data cache and prefetching"
echo "z     Proxy routing"
}

# DEFAULT VALUES FOR THE PARAMETERS

## Simulator Run Parameters
default=99
bin="$default"  # Name of the binary
verbose=0       # Minimal output
assert_mode=0   # No assertions
folder="sim_logs" # Folder to store the simulation results
sbatch_folder="sim_sbatch" # Folder to store the SBATCH logs (only for cluster runs using SLURM)
dataset_folder="datasets"
max_threads=16  # Maximum number of simulation threads in the host
local_run=0     # Run on a cluster using SLURM. NOTE that 0: cluster, 1: local (background), 2: local (foreground)
let powers_sample_time=0 # Power of 2 of the sample time
let functional=1 # Functional simulation
let dry_run=0   # Dry run (no simulation, only print configuration)


## Design-Under-Test (DUT) Design Parameters
let torus=1     # NoC topology (0:mesh, 1:torus)
express_link=$default  # express_link length (in number of tiles). Default: not set it based on this option, but whatever is in src/common/macros.h
let phy_nocs=1  # Number of physical NoCs
let noc_conf=2  # NoC configuration. NOC_CONF:0 means #P NoCS of 32 bits intra die, which get throttled to a shared 32 bits across dies; NOC_CONF:1 means #P-1 NoCS of 32 and one of 64 bit intra die, which get throttled to a shared NoC 32 bits across dies; NOC_CONF:2 means #P-1 NoCS of 32 and one of 64 bit intra die, which get throttled to #P NoCs of 32 bits across dies; NOC_CONF:3 means #P-1 NoCS of 32 and one of 64 bit intra and inter-die. No throttling. Note that when P==1, then NOC_CONF 1 and 2 are the same.
let grid_x=8    # Default grid size 8x8\
let chiplet_w=0
let board_w=0
let sram_memory=256*512 # 512 KiB (256 words of 64 bits is 1 KiB)
let dcache=0    # Whether to use a data cache (0: no dcache, only scratchpad, 1: dcache if not fit on scratchpad, 2: always dcache, 3: only prefetching and spatial locatity, 4: spatial locality only, 5: everything is a miss)


## Software Configurations of the DUT
let loop_chunk=64 # How many iterations T1 can run before splitting into several tasks
let queue_prio=0  # Deprecated (only option 0 available, TSU is always on, no RR)
let large_queue=0 # Queue size experiments (0: default, 1+ experiment increases sizes)
let proxy_w=0   # Proxy, Chiplet and Board sizes unset by default (set later based on default values if not explicitly set)
let force_proxy_ratio=0 # Whether we for the Proxy Cache to a particular ratio wrt the Proxy Array. Not forced by default.
proxy_routing=4 # 3: NoCascading, 4:SelectiveCascading, 5:AlwaysCascading
write_thru=$default # Set default based on App (below)
barrier=$default
steal=0 # Steal regions (0: no steal, 1+: steal region size, pow2)

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
    # i) steal=${OPTARG};;
    j) write_thru=${OPTARG};;
    k) board_w=${OPTARG};;
    l) express_link=${OPTARG};;
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

# Helper function to calculate log2
function log2 {
    local x=0
    for (( y=$1-1 ; $y > 0; y >>= 1 )) ; do
        let x=$x+1
    done
    echo $x
}
let log2grid_x=$(log2 $grid_x)

############################################################
run_batch(){
  echo \#APP=$a
  echo \#DATASET=$d
  echo \#MESH=$grid_x-x-$grid_x

  d_sub=${d:0:1}
  d_sub2=${d:4:6}
  bin_name="${bin}a${a}g${grid_x}-${d_sub}${d_sub2}.run"

  barrier_post=$barrier
  binary="bin/$bin_name"
  if [ $barrier_post -eq $default ]; then # If barrier not defined in the options, set it based on the app
    if [ $a -eq 1 ] || [ $a -eq 10 ]; then
      barrier_post=1
    else
      barrier_post=0
    fi
  fi


  write_thru_post=$write_thru
  # 1: Write Thru, 0: Write Back
  # If variable not set in the options, give default value for different apps!! If set, then use that value!
  if [ $write_thru_post -eq $default ]; then
    if [ $a -eq 1 ] || [ $a -eq 4 ] || [ $a -eq 5 ] || [ $a -eq 7 ] || [ $a -eq 8 ]; then
      #Apps without frontier, or with barrier, use write back policy
      write_thru_post=0
    else # Apps: 0 2 3, i.e., SSSP, BFS, WCC, use the write thru policy
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

  # If no proxy, we make sure the proxy routing is off!
  if [ $proxy_w -eq $grid_x ]; then 
    proxy_routing=0
  fi

  threads=1 # Round down to the nearest power of 2
  while [ $((threads * 2)) -le $max_threads ]; do
    threads=$((threads * 2))
  done
  if [[ $grid_x -lt $threads ]]; then
    threads=$grid_x*2
  fi
  echo \#MAX_THREADS=$max_threads
  echo \#SIMULATOR_THREADS=$threads

  options=""
  if [ $phy_nocs -gt 1 ]; then
    options="-DPHY_NOCS=$phy_nocs"
  fi
  
  # Macros not actively used: -DSTEAL_W=$steal_w -DSHUFFLE=$shuffle -DQUEUE_PRIO=$queue_prio 

  options="-DMAX_THREADS=$threads -DASSERT_MODE=$assert_mode -DPRINT=$verbose -DDCACHE=$dcache -DSRAM_SIZE=$sram_memory -DGLOBAL_BARRIER=$barrier_post -DTORUS=$torus -DGRID_X_LOG2=$log2grid_x -DAPP=$a -DDIE_W=$chiplet_w -DDIE_H=$chiplet_h -DPROXY_W=$proxy_w -DTEST_QUEUES=$large_queue -DWRITE_THROUGH=$write_thru_post -DNOC_CONF=$noc_conf -DLOOP_CHUNK=$loop_chunk -DPROXY_ROUTING=$proxy_routing -DFUNC=$functional $options"
  
  if [ $force_proxy_ratio -gt 0 ]; then
    options="$options -DFORCE_PROXY_RATIO=$force_proxy_ratio"
  fi

  # Only set express_link if it is not the default value
  if [ $express_link -lt $default ]; then
    options="$options -DRUCHE_X=$express_link"
  fi

  # Default value for $board_w is $grid_x
  if [ $board_w -gt 0 ]; then
    options="$options -DBOARD_W=$board_w"
  fi

  compiler=${CXX:-g++} # It has been tested with clang++ g++ or icpx (Intel CPUs)
  par_lib="-lpthread" #Alternatively could use OpenMP by setting: par_lib="-fopenmp -DUSE_OMP=1"

  compiler_args=""; target=""
  cluster=0  # 0:any, 1: intel-cascade-64, 2:intel-icelake-32,  3:amd
  if [ $local_run -eq 0 ]; then
    if [ $grid_x -gt 256 ] || [ $max_threads -gt 48 ]; then
      cluster=1
    else
      cluster=2
    fi
    if [ $cluster -eq 1 ]; then
      target="cascade"; compiler_args=" -march=cascadelake -mtune=cascadelake"
    elif [ $cluster -eq 2 ]; then
      target="icelake"; compiler_args=" -march=$target-server -mtune=$target-server"
    elif [ $cluster -eq 3 ]; then
      compiler_args=" -march=znver1 -mtune=znver1"
      target="rome"
    fi
  fi
  compile="$compiler src/main.cpp $par_lib ${compiler_args} -o $binary $options -O3 -std=c++11"
  echo $compile
  $compile
  output_file="DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
  echo $output_file

  # check if local_run > 0
  if [ $local_run -gt 2 ]; then
    echo "Error: local_run must be 0, 1 or 2"
  elif [ $local_run -gt 0 ]; then
    #result_file="RES-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
    cmd="$binary $dataset_folder/$d/ > $folder/$output_file"
    echo $cmd; echo
    #check if local_run is 1 or 2
    if [ $local_run -eq 1 ]; then
      bin/$bin_name $dataset_folder/$d/ $bin $dry_run > $folder/DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log &
    else
      bin/$bin_name $dataset_folder/$d/ $bin $dry_run output.txt > $folder/DATA-$d--$grid_x-X-$grid_x--B$bin-A$a.log
    fi
  else # cluster (local_run=0)
    hours=24
    total_mem=512
    if [ $grid_x -gt 512 ]; then
      hours=144 # 144
      total_mem=2048
    elif [ $grid_x -gt 256 ]; then
      hours=72
      total_mem=1024
    elif [ $grid_x -gt 128 ] || ([ $grid_x -gt 64 ] && [ $a -eq 1 ]); then
      hours=72
      total_mem=512
    fi
    
    mkdir -p $sbatch_folder
    slurm_file="SLURM-$d--$grid_x-X-$grid_x--B$bin-A$a.log"
    echo "#!/bin/bash"                        > batch.sh
    echo "#SBATCH --time=$hours:00:00"           >> batch.sh
    echo "#SBATCH --output=$sbatch_folder/$slurm_file" >> batch.sh
    echo "#SBATCH --nodes=1"               >> batch.sh
    echo "#SBATCH --ntasks=1"              >> batch.sh
    echo "#SBATCH --cpus-per-task=$max_threads"   >> batch.sh
    echo "#SBATCH --job-name=A$a-G$grid_x-$d-B$bin" >> batch.sh
    if [ $cluster -gt 0 ]; then
      echo "#SBATCH --constraint=$target  "   >> batch.sh
      if [ $cluster -gt 1 ]; then
          echo "#SBATCH --gres=gpu:a100:0" >> batch.sh
          echo "#SBATCH --mem-per-cpu=16G" >> batch.sh
      else # cluster==1
          echo "#SBATCH --mem=${total_mem}G" >> batch.sh
      fi
    fi
    #echo "#SBATCH --mail-type=begin"       >> batch.sh        # send email when job begins
    #echo "#SBATCH --mail-type=end"          >> batch.sh        # send email when job ends
    #echo "module load gcc-toolset/10"       >> batch.sh
    cmd="srun ./$binary $PWD/$dataset_folder/$d/ $bin $dry_run > $folder/$output_file"
    echo $cmd; echo $cmd >> batch.sh; sbatch batch.sh;
    squeue -u `whoami` --format="%.18i %.9P %.50j %.8u %.2t %.10M %.6D %R"
  fi
}



if [ -z "$datasets" ]; then
  echo "Error: DATASETS (\$d) must be specified."
  exit 1
fi
if [ -z "$apps" ]; then
  echo "Error: APPS (\$a) must be specified."
  exit 1
fi

for d in $datasets
do
  for a in $apps
  do
    echo "Running $a with $d"
    if [[ -z "$a" || -z "$d" ]]; then
      echo "Error: Both APP (\$a) and DATASET (\$d) must be specified."
      exit 1
    fi
    run_batch
  done
done

