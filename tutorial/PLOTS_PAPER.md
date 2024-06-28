# Plotting Paper Figures

## Simulator Validation

    python3 plots/characterization.py -e 1 -p 18

## Figure 2 - Heatmaps

    // Generate plots for configurations HEATMAPS

    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT0 --nogui
    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT0 --nogui -m 'FMCore Active'
    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT1 --nogui
    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT1 --nogui -m 'FMCore Active'
    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT2 --nogui
    python3 gui/visualization.py -e 1 -p Heatmap -n HEAT2 --nogui -m 'FMCore Active'

## Figure 3 - Runtimes

    python3 plots/characterization.py -e 1 -p 17 -m 11


## Figure 4 - Scaling plots

    python3 plots/characterization.py -e 2 -p 6 -m 3

## Figure 5 - Memory Integration Case Study

    python3 plots/characterization.py -e 1 -p 9 -m 0
    python3 plots/characterization.py -e 1 -p 9 -m 7
    python3 plots/characterization.py -e 1 -p 9 -m 10


# Simulation Runs for paper results

## Simulator Validation

    exp/run_fft.sh 32
    exp/run_fft.sh 64
    exp/run_fft.sh 128
    exp/run_fft.sh 256
    exp/run_fft.sh 512

## Figure 2 - Heatmaps

    exp/run_exp_heatmap.sh 2 0 2 64 Kron22

## Figure 3 - Runtimes

    exp/run_exp_sim_time.sh 9 0 4 32 Kron22
    exp/run_exp_sim_time.sh 9 0 4 64 Kron22


## Figure 4 - Scaling plots

    exp/run_exp_scaling_1m.sh 9 0 0

## Figure 5 - Memory Integration Case Study

    exp/run_exp_mem.sh 9 0 5 Kron25