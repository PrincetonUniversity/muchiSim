#/bin/bash

rm -rf plots/animated_heatmaps/2024*
rm -rf plots/images/*.png 
rm -rf plots/temp/TEMP*
rm -rf sim_logs/*.log
rm output.txt
rm batch.sh
rm -rf sim_sbatch 
rm -rf sim_counters
rm -rf plots/characterization/*.pdf
rm -rf bin/*.run
source $MUCHI_ROOT/setup.sh