# Plotting Paper Figures

## Simulator Validation

    python3 plots/characterization.py -p 18

## Figure 2 - Heatmaps

    // Generate plots for configurations HEATMAPS
    python3 gui/gui.py

## Figure 3 - Runtimes

    python3 plots/characterization.py -p 17 -m 11


## Figure 4 - Scaling plots

    python3 plots/characterization.py -p 6 -m 3

## Figure 5 - Memory integration Case Study

    python3 plots/characterization.py -p 9 -m 0
    python3 plots/characterization.py -p 9 -m 7
    python3 plots/characterization.py -p 9 -m 10


# Simulation Runs for paper results

## Simulator Validation

    exp_paper/run_fft.sh 32
    exp_paper/run_fft.sh 64
    exp_paper/run_fft.sh 128
    exp_paper/run_fft.sh 256
    exp_paper/run_fft.sh 512

## Figure 2 - Heatmaps

    exp_paper/run_exp_heatmap.sh 2 0 2 64 Kron22

## Figure 3 - Runtimes

    exp_paper/run_exp_sim_time.sh 9 0 4 32 Kron22
    exp_paper/run_exp_sim_time.sh 9 0 4 64 Kron22


## Figure 4 - Scaling plots

    exp_paper/run_exp_scaling_1m.sh 9 0 0

## Figure 5 - Memory integration Case Study

    exp_paper/run_exp_mem.sh 9 0 5 Kron25



# Plotting Other Possible Case Studies not included in the paper

## Study - NoC

    python3 plots/characterization.py -p 19 -m 0
    python3 plots/characterization.py -p 19 -m 7
    python3 plots/characterization.py -p 19 -m 10



## Study - Granularity, PU/Tile

    python3 plots/characterization.py -p 23 -m 0
    python3 plots/characterization.py -p 23 -m 7


## Study - PU Frequency

    python3 plots/characterization.py -p 20 -m 0
    python3 plots/characterization.py -p 20 -m 7


## Study - Integrating HBM

    python3 plots/characterization.py -p 2
    python3 plots/characterization.py -p 3


## Study - Energy Breakdown

    python3 plots/characterization.py -p 1 -m 0
    python3 plots/characterization.py -p 1 -m 1


## Study - Scaling Plot

    python3 plots/characterization.py -p 4 -m 0
    python3 plots/characterization.py -p 4 -m 1


## Study - Baseline vs Reduction Tree scaling

    python3 plots/characterization.py -p 14 -m 0
    python3 plots/characterization.py -p 14 -m 8


## Study - Cascading

    python3 plots/characterization.py -p 21 -m 0
    python3 plots/characterization.py -p 21 -m 7
    python3 plots/characterization.py -p 21 -m 8


## Study - Proxy Size

    python3 plots/characterization.py -p 12 -m 0
    python3 plots/characterization.py -p 12 -m 7
    python3 plots/characterization.py -p 12 -m 8


## Study - Proxy Cache Pressure

    python3 plots/characterization.py -p 11 -m 0
    python3 plots/characterization.py -p 11 -m 7
    python3 plots/characterization.py -p 11 -m 8


## Study - Sync Characterization

    python3 plots/characterization.py -p 24 -m 0
    python3 plots/characterization.py -p 24 -m 7

## Study - Reduction Tree with different Networks

    python3 plots/characterization.py -p 27 -m 0
    python3 plots/characterization.py -p 27 -m 7

## Study - Heatmap

    // Generate plots for configurations HEATMAPS
    python3 gui/gui.py
    

## Study - Scaling plots

    python3 plots/characterization.py -p 6 -m 0
    python3 plots/characterization.py -p 6 -m 2





# Simulation Runs for the Other Case Studies not included in the paper


## Study - NoC

    exp/run_exp_noc_bw.sh 9 0 4 64


## Study - Granularity, PU/Tile

    exp/run_exp_granularity.sh 9 0 2 64


## Study - PU Frequency

    exp/run_exp_pufreq.sh 9 0 4 64


## Study - Packages

    exp/run_exp_packages.sh 9 0 3 Kron25
    exp/run_exp_packages.sh 9 0 3 Kron26


## Study - Scaling Plot

    exp/run_exp_scaling_dcra.sh 9 1 3 Kron25
    exp/run_exp_scaling_dcra.sh 9 1 3 Kron26



## Study - Baseline vs Reduction Tree scaling

    exp/run_exp_dlx_scale.sh 9 0 3


## Study - Cascading

    exp/run_exp_cascading.sh 9 0 2 128


## Study - Proxy Size

    exp/run_exp_proxy.sh 9 0 5 128


## Study - Proxy Cache Pressure

    exp/run_exp_pcache.sh 9 0 2 128


## Study - Sync Characterization

    exp/run_exp_sync.sh 9 0 1 128


## Study - Reduction Tree with different Networks

    exp/run_exp_proxy_w_noc.sh 9 0 3 128

## Study - Heatmap

    exp/run_exp_heatmap.sh 5 0 1 64 Kron22


## Study - Scaling plots

    exp/run_exp_scaling_1m.sh 9 0 5
