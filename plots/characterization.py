#!/usr/bin/python

import matplotlib as mpl
from matplotlib.ticker import AutoMinorLocator
from parse import *
mpl.use('pdf')
import matplotlib.gridspec as gridspec

import matplotlib.pyplot as plt
from matplotlib import rc
import numpy as np
import sys, getopt
plt.rcParams["font.family"] = "Times New Roman"
rc('font',**{'family':'serif','serif':['Times New Roman']})
rc('text', usetex=True)

folder = ["sim_logs","sim_paper"]
font = 20
font_xticks = font - 1
leg_inputs = {'ego-Twitter':'TT', 'liveJournal':'LJ', 'amazon-2003':'AZ', 'wikipedia':'WK', 'Kron16':'R16', 'Kron22':'R22', 'Kron25':'R25', 'Kron26':'R26', 'fft1':'FFT'}

default_grid_width = 64

def geo_mean(iterable):
    a = np.array(iterable)
    return a.prod()**(1.0/len(a))

def collect_data(binaries, metrics, apps, inputs,plot_type, plot_metric):
    data_planes = []
    data_metric = []
    for metric_name in metrics:
        pattern_index = metric_of_interest[int(metrics_names[metric_name])]
        data_apps = []
        for app in apps:
            print("APP: "+app)
            data_dataset = []
            for dataset in inputs:
                data_bin = []
                keys = list(leg_inputs.keys())
                #Support for plot increasing grid sizes
                if dataset not in keys:
                    print("Dataset not in keys: "+dataset)
                    if "--" in dataset:
                        strings = dataset.split("--")
                        dataset=strings[0]
                        grid=int(strings[1])
                        print("1-Split Dataset: "+dataset)
                        print("1-Split Grid: "+str(grid))
                    else:
                        grid = int(dataset) # In this case the input contains the grid size, not the dataset
                        if plot_type==4 or plot_type==6:
                            dataset="Kron26"
                        else:
                            print("ERROR: Dataset not in keys and not a grid size: "+dataset)
                            sys.exit(2)
                else: # if dataset is in keys, then default grid size
                    grid = default_grid_width

                print(dataset)
                print(grid)
                for binary in binaries:
                    print(binary)
                    # Split binary into type and grid config
                    if "--" in binary:
                        strings = binary.split("--")
                        binary=strings[0]
                        grid=int(strings[1])
                        print("2-Split Dataset: "+dataset)
                        print("2-Split Grid: "+str(grid))

                        if (plot_type>=1) and (plot_type<=3) and (dataset=="Kron25"):
                            grid=grid>>1
                            
                        if (plot_type>=1) and (plot_type<=3) and (dataset=="Kron25") and ((binary=="SCL2")):
                            binary="MEM6"
                        elif (plot_type>=1) and (plot_type<=3) and (dataset=="Kron26") and ((binary=="SCL2")):
                            binary="DCRA32_S"

                    if (plot_type==4) and (dataset=="Kron26") and (app=="pagerank") and ((grid==64) or (grid==32)):
                        binary="DCRA32_SB"
                    elif (plot_type==4) and (dataset=="Kron26") and (grid==32):
                        binary="SCL2"
                    elif (plot_type==4) and (dataset=="Kron26") and (grid==256):
                        binary="SCL1"

                    if (plot_type==6) and (app=="pagerank") and (grid<512):
                        binary="SCALINGB"
                    if (plot_type==6) and (app=="fft"):
                        binary="B3"; dataset="fft1"
                    if (app=="histo" or app=='pagerank' or app=="spmv") and (binary=="THRU0"):
                        binary="1PMCCASC0"

                    print("Binary Name: "+binary)

                    value = -1
                    f = 0
                    while (value == -1) and f<len(folder):
                        found,filename,temp = search(folder[f],dataset, grid, binary,app)
                        f = f+1
                        if found==1:
                            value = read_file(filename,temp,pattern_index)
                            print("Value: "+str(value))
                            if plot_metric==9 and value==-1:
                                value = 100
                        else: #File not found
                            value = -1
                    # Finished searching on all folders
                    if (found == 0):
                        print("ERROR: FILE NOT FOUND FOR: "+binary+", on "+filename)
                        if plot_type==6:
                            value = None
                        else:
                            sys.exit(2)
                    if (value == -1):
                        print("ERROR: VALUE NOT FOUND FOR: "+binary+", on "+filename)
                        sys.exit(2)

                    data_bin.append(value)
                    print(value)

                data_dataset.append(data_bin)
            data_apps.append(data_dataset)
        print(patterns[pattern_index])
        print(data_apps)
        data_metric.append(data_apps)

    print("\nALL METRICS:")
    print(data_metric)

    b=0
    for binary in binaries:
        print("\n"+binary)
        for metric_int in range(0,len(metrics)):
            data_from_this_binary = []
            a=0
            for app in apps:
                print("\n"+app)
                output = []
                for d in range(0,len(inputs)):
                    print("Dataset idx: "+str(d))
                    #metric, app, dataset, grid
                    data = data_metric[metric_int][a][d][b]
                    print("Data: "+str(data))
                    if data != None:
                        output.append(float(data))
                    else:
                        output.append(float('nan'))
                a=a+1
                data_from_this_binary.append(output)
            data_planes.append(data_from_this_binary)
        b=b+1
    return data_planes

def main(argv):
    global default_grid_width
    usage = "characterization.py -p <plot_type> -m <metric> (0:time, 1:utilization)"
    plot_type = 0
    plot_len = 0
    plot_metric = 0

    try:
        opts, args = getopt.getopt(argv,"hp:m:")
    except getopt.GetoptError:
        print(usage)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print(usage)
            sys.exit()
        elif opt in ("-p", "--plot_type"):
            plot_type = int(arg)
        elif opt in ("-m", "--metric"):
            plot_metric = int(arg)

    # Num cols of the leyend of the plot
    cols=6
    metrics = None

    apps = ['sssp','pagerank','bfs','wcc','spmv','histo']

    colors = [
    'lightskyblue',
    'dodgerblue',  # a fresh green
    'gold',  # a vibrant shade of blue
    'darkorange',
    'mediumseagreen',
    'orchid', 
    'darkgreen',
    'mediumpurple',
    'navy' 
    ]

    hatches = ['','\\','.','//','o','-','x','*','O']

    perf_word = "TEPS"
    perf_metric = "TEPS"
    perf_label = "TEPS"
    energy_label = "TEPS/Watt"
    #plot_type<=4 or plot_type==10 or plot_type==9 or plot_type==19 or plot_type==20 or plot_type==23:
    if plot_type==9: 
        perf_word = "FLOPS"
        perf_metric = "flops"
        perf_label = "FLOPS" #"Performance"
        energy_label = "FLOPS/Watt" #Energy Efficiency"

    legend_left_point = 0
    app_cols = 2
    legend_top_sep = -0.22
    legend_height = 0.1
    legend_width = 6.5

    if (plot_type>=9):
        #Default values for Plot types > 10
        y_lim = 10
        inputs = ['wikipedia','Kron22']
                
        if plot_type==9: #SRAM size comparison
            y_lim = 13
            default_grid_width = 32
            # inputs = ['Kron22','Kron25']; binaries = ["MEM0","MEM1","MEM2","MEM6","MEM3","MEM4","MEM5"]; app_cols = 2; cols=4
            #comp = ("128T/C 64KB","128T/C 128KB","128T/C 256KB","128T/C 512KB","32T/C 128KB","32T/C 256KB","32T/C 512KB")
            inputs = ['Kron25']; binaries = ["MEM0","MEM1","MEM2","MEM3","MEM4","MEM5"]; app_cols = 1; cols=3; legend_top_sep=-0.11
            apps = ['sssp','pagerank','bfs','wcc','spmv','spmm','histo']
            comp = ("128T/C 64KB","128T/C 128KB","128T/C 256KB","32T/C 128KB","32T/C 256KB","32T/C 512KB")
            legend_width = 6.8
            legend_left_point = -0.6
        elif plot_type==10: #QUEUE size comparison
            y_lim = 2.5
            binaries = ["OQUEUE1","OQUEUE3","OQUEUE4","OQUEUE5","OQUEUE6"]
            comp = ("OQ2=OQ1","OQ2=OQ1*4","OQ2=OQ1*8","OQ2=OQ1*16","OQ2=OQ1*32")
            apps = ['sssp','pagerank','bfs','wcc','spmv']
            cols=3
            legend_width = 5.5
        elif plot_type==11: #PROXY cache
            y_lim = 10.5
            default_grid_width = 128
            binaries = ["PROXY128", "CASC2","NPCACHESCL16F2","NPROXY16F","NPCACHESCL16F8","CASCF162"]
            comp = ("No Proxy","16x16 P\$ in Full","16x16 P\$ 1/2","16x16 P\$ 1/4","16x16 P\$ 1/8","16x16 P\$ 1/16")
            cols=3
        elif plot_type==12: #PROXY size comparison
            y_lim = 11
            default_grid_width = 128
            binaries = ["PROXY128","PROXY32","NPROXY16F","NPROXY8F","CASC2","NPROXY8"]
            comp = ("No Proxy","Proxy 32x32","Proxy 16x16","Proxy 8x8")
            cols=4
            legend_width = 6.8

        elif plot_type==13: #STEAL REGION comparison
            binaries = ["S16S0X0","S16S0X1","S16S1X0","S16S1X1"]
            comp = ("S16S0X0","S16S0X1","S16S1X0","S16S1X1")

        elif plot_type==14: #Baseline vs Reduction
            y_lim = 12
            apps = ['sssp','pagerank','bfs', 'wcc','spmv','histo']
            binaries = ["DLXSD64--64","NDLXST64--64","PROXY128--128","CASC2--128","DLXSD256--256","NDLXST256--256"]
            comp = ("Baseline-64x64","Reduction-64x64","Baseline-128x128","Reduction-128x128","Baseline-256x256","Reduction-256x256")
            cols=3
        elif plot_type==15: #NETWORK
            y_lim = 18
            #inputs = ['Kron22']
            apps = ['bfs','spmv','histo']
            binaries = ["NOCWN0","NOCWN1","NOCWN2","NOCWN6"]
            comp = ("(a) Mesh 32", "(b) Torus 32", "(c) 64:32", "(d) +Die-Skip")
            cols=2
        elif plot_type==16: #PREFETCHING
            y_lim = 4
            binaries = ["DMBC6","DMBC5","DMBC4","DMBC3","DMBC1"]
            comp = ("Always HBM","Cache last request","Remote SRAM","Local SRAM","Always SRAM")
            legend_width = 3.2
            cols=3
        elif plot_type==17: #Simulation time
            y_lim = 10
            apps = ['sssp','pagerank','bfs','spmv','spmm','histo', 'fft']
            # inputs = ['Kron25--32','Kron25--64']
            #inputs = ['Kron22--16',"Kron22--32","Kron22--64"]
            inputs = ["Kron22--32","Kron22--64"]
            binaries = ["ITHR2","ITHR4","ITHR8","ITHR16","ITHR32"]
            comp = ("2 Threads","4 Threads","8 Threads","16 Threads","32 Threads")
            legend_width = 7.5
        elif plot_type==18: #FFT validation
            y_lim = 2
            apps = ['fft']
            inputs = ['fft1']
            #binaries = ["SCALING--32","SCALING--64","SCALING--128","SCALING--256", "SCALING--512"]; inputs = ['Kron26']
            # binaries = ["B2--32","B2--64","B2--128","B2--256", "B2--512"]
            binaries = ["B3--32","B3--64","B3--128","B3--256", "B3--512"]
            comp = ("32","64","128", "256","512")
            legend_width = 3
            cols=3
        elif plot_type==19: #NOC_TYPES
            default_grid_width = 64
            y_lim = 16
            inputs = ['Kron22','wikipedia']
            binaries = ["NOCM0","NOCM1","NOCM2","NOCM3","NOCM4"]
            comp = ("Mesh 32-bit",  "Mesh 64-bit",  "Torus 64-bit", "+inter-die-NoC 32-bit","NoC Freq=2Ghz") #, "+Ruche")
            cols=3    
        elif plot_type==20: #PU_FREQ
            default_grid_width = 64
            y_lim = 10
            binaries = ["PUF0","PUF1","NOCM3","PUF4"]
            comp = ("PU=0.25Ghz","PU=0.5Ghz","PU=1.0Ghz","PU=2.0Ghz")
            legend_width = 6.5
            cols=4
        elif plot_type==21: # Cascade Policy
            default_grid_width = 128
            y_lim = 10
            binaries = ["PROXY128","THRU0","CASC1","CASC2"]
            cols=2
            comp = ("Baseline","Proxy \& Merge Owner","Proxy \& Cascade","Reduction")
        elif plot_type==22: # Write Thru
            apps = ['sssp','bfs','wcc']
            default_grid_width = 128
            y_lim = 2
            binaries = ["1PMCCASC0","CASC2","THRU0","THRU1","THRU2"]
            comp = ("WT Never", "WT Selective","WB Never", "WB Always", "WB Selective")
            cols=3
        elif plot_type==23: #GRANULARITY
            #default_grid_width = 64
            y_lim = 2.5
            #apps = ['sssp','bfs', 'wcc','spmv','histo']
            binaries = ["GRANU0--64","GRANU1--32","GRANU2--16"]
            comp = ("1PU/Tile 64x64Tiles",  "4PU/Tile 32x32Tiles",  "16PU/Tile 16x16Tiles")
            cols=3
            legend_width = 6.8
        elif plot_type>=24 and plot_type <=27 : # Sync advantage
            default_grid_width = 128
            if plot_type==24:
                y_lim = 10
                binaries = ["PROXY128","SYNC0","SYNCUP1","THRU0","CASC2"]
                comp = ("No Proxy","Proxy Sync \& Merge","Proxy Sync \& Cascade","Proxy Merge","Reduction")
                cols=2
            elif plot_type==25:
                y_lim = 13
                colors[2] = colors[3];hatches[2] = hatches[3]
                binaries = ["PX_NOC0","PX_NOC2","PX_NOC1"]
                comp = ("No Proxy (Mesh)","Proxy Sync \& Merge","Reduction")
            elif plot_type==26:
                colors[2] = colors[3];hatches[2] = hatches[3]
                binaries = ["PX_NOC3","PX_NOC4","PX_NOC5"]
                comp = ("No Proxy (Inter-chip)","Proxy Sync \& Merge","Reduction")
            elif plot_type==27:
                c1 = colors[1]; colors[1] = colors[3]; h1 = hatches[1]; hatches[1] = hatches[3]; colors[3] = c1; hatches[3] = h1
                c1 = colors[2]; colors[2] = colors[3]; h1 = hatches[2]; hatches[2] = hatches[3]; colors[3] = c1; hatches[3] = h1
                binaries = ["PROXY128","CASC2","PX_NOC0","PX_NOC1","PX_NOC3","PX_NOC5"]
                comp = ("No Proxy","Reduction (Torus)", "No Proxy (Mesh)","Reduction (Mesh)", "No Proxy (Inter-chip)","Reduction (Inter-chip)")
                cols=3


        #whether we show utilization or normalized performance
        if metrics == None:
            if plot_metric==0:
                metrics = ["time"]
            elif plot_metric==1:
                metrics = ["Core_active"]
            elif plot_metric==2:
                metrics = ["router_energy"]
            elif plot_metric==3:
                metrics = ["sim_time"]
            elif plot_metric==4:
                metrics = ["arith_intensity_msgs"]
            elif plot_metric==5:
                metrics = ["arith_intensity_loads"]
            elif plot_metric==6:
                metrics = ["flops"]
            elif plot_metric==7:
                metrics = ["energy"]
            elif plot_metric==8:
                metrics = ["total_msg"]
            elif plot_metric==9:
                metrics = ["dhit_rate"]
            elif plot_metric==10:
                if plot_type==19:
                    metrics = [perf_metric,"cost_2d"]
                else:
                    metrics = [perf_metric,"cost_2.5d"]
            elif plot_metric==11:
                metrics = ["sim_time","time"]

        plot_len = len(binaries)
        bars = len(inputs)+len(comp)/1.1

    elif (plot_type==1): #ENERGY BREAKDOWN!!!
        if plot_metric==0:
            binaries = ["SCL1--256"]
        else:
            binaries = ["SCL2--64"]
        metrics = ["core_energy","mem_energy","router_energy"]
        plot_len = len(metrics)
        apps = ['sssp','bfs', 'wcc','spmv','histo']
        #apps = ['histo']
        inputs = ['Kron25','Kron26']
        comp = ('Logic', 'Memory', 'Network')
        legend_width = 4.8
        bars = len(inputs)+len(comp)/1.1

    # "PACKAGES" experiment, comparing Reduction-SRAM, Baseline and Reduction-HBM
    elif (plot_type==2 or plot_type==3): #perf/$ or energy/op
        binaries = ["SCL1--256","SCL3--128","SCL2--64"] #,"SCL2--64"] # Normalized to DSIM
        plot_len = len(binaries)
        apps = ['sssp','bfs','wcc','spmv','histo']
        inputs = ['Kron25','Kron26']
        cols=3
        #comp = ('DSIM SRAM','Baseline','DSIM 2.5D-HBM (Horizontal)','DSIM 3D-HBM (Vertical)')
        comp = ('DSIM SRAM','Baseline','DSIM-HBM')
        colors[2] = colors[3];hatches[2] = hatches[3]
        legend_width = 5
        bars = len(inputs)+len(comp)/1.1
        if plot_type==2:
            y_lim = 4.5
            label=perf_word+"/\$ Improvement"
            metrics = [perf_metric,"cost_2d","cost_2.5d","cost_3d"]
        else:
            y_lim = 2
            label=perf_word+'/Watt Improvement'
            metrics = ["energy","3d_energy","cost_2d","cost_2.5d","cost_3d"]

    elif (plot_type==4) or (plot_type==6): #MASSIVE SCALING DSIM
        if plot_type==4:
            apps = ['sssp','pagerank','bfs', 'wcc','spmv','histo']
            binaries = ["DCRA32_S"]; inputs = ['32','64','128','256'] #Kron26, chiplet 32x32
        else:
            binaries = ["SCALING"]
            apps = ['sssp','pagerank','bfs', 'wcc','spmv','histo']#,'fft']
            inputs = ['32','64','128','256','512','1024']

        if plot_metric==0:
            metrics = [perf_metric,"gops","effective_bw"]
            comp = [perf_metric, "Operations/s", "Avg. Mem. BW (Bytes/s)"]
            legend_width = 6.5
        elif plot_metric==1:
            metrics = [perf_metric,"power","cost_2.5d","total_msg"]
            comp = [perf_word+"/\$", perf_metric+"/Watt", "NoC Messages"]
            legend_width = 6
        elif plot_metric==2:
            metrics = [perf_metric,"gops","power"]
            comp = [perf_word+"/Watt", "Operations/s/Watt"]
            legend_width = 5
        elif plot_metric==3:
            metrics = ["sim_time","time","gops","total_msg"]
            comp = ["1","2","3"]
            legend_width = 5.5

        plot_len = len(metrics)
        
        bars = len(inputs)/2+len(binaries)


    # COLLECT DATA
    assert(metrics != None)
    data = collect_data(binaries,metrics,apps,inputs,plot_type, plot_metric)
    geomean_per_app = []

    fig_width = max(2*len(apps), 2*len(inputs))
    fig_height = 3.5
    plt.figure(figsize=(fig_width, fig_height)) #size of the global figure
    gs = gridspec.GridSpec(1, len(apps) * app_cols + 1)
    plt.subplots_adjust(left=None, bottom=None, right=None, top=None, wspace=0.10, hspace=0.0000001)
    
    line_for_legend = []
    legend_labels = []
    # PRINT PLOTS j is apps
    for j in range(len(apps)):
        accum_energy = []
        ax = plt.subplot(gs[0, j * app_cols:j * app_cols + app_cols],frameon=False)  # Full width
        plt.rcParams["font.family"] = "Times New Roman"
 
        if plot_type==0: #network comparison
            plt.ylim([0, 3])
            label = perf_label+" Improvement"
        elif plot_type>=9:
            if metrics[0]=="Core_active":
                plt.ylim([0, 100])
                label='PU Utilization'
            elif metrics[0]=="dhit_rate":
                plt.ylim([0, 100])
                label='Hit Rate (\%)'
            elif metrics[0]=="energy":
                if plot_type==9:
                    plt.ylim([0, 4])
                elif plot_type==20:
                    plt.ylim([0.75, 1.25])
                else:
                    plt.ylim([0, 2])
                label = energy_label +" Improvement"
            elif metrics[0]=="total_msg":
                label='Reduction in NoC traffic'
                if plot_type==12:
                    plt.ylim([0, 3.3])
                elif plot_type==14: # Scaling Baseline vs Reduction
                    ax.set_yscale('log'); plt.ylim([0.1, 20])
                    label='Increase in NoC Traffic w/ Scale'
                elif plot_type==21:
                    plt.ylim([0, 3.3])
                else:
                    plt.ylim([0, 3.3])
            elif plot_metric==11:
                plt.yscale('log')
                if plot_type==14:
                    plt.ylim([10, 1000])
                    size1 = 64*64
                    size2 = 128*128
                    size3 = 256*256
                elif plot_type==17:
                    plt.ylim([10, 2000])
                    size1 = 16*16
                    size2=size1; size3=size1
                else:
                    plt.ylim([1, 2000])
                    size1=default_grid_width*default_grid_width
                    size2=size1; size3=size1
                print("RANGE: "+str(len(inputs)))
                # K is the number of datasets
                for k in range(0, len(inputs)):
                    if plot_type==17:
                        size1 *= 4; size2=size1; size3=size1
                        print("GRID SIZE: "+str(size1))
                    data[0][j][k] = data[0][j][k] * 1000000.0 / (data[1][j][k]  * size1)
                    data[1][j][k] = data[2][j][k] * 1000000.0 / (data[3][j][k]  * size1)
                    if len(data) > 4:
                        data[2][j][k] = data[4][j][k] * 1000000.0 / (data[5][j][k]  * size2)
                        data[3][j][k] = data[6][j][k] * 1000000.0 / (data[7][j][k]  * size2)
                        if len(data) > 8:
                            data[4][j][k] = data[8][j][k] * 1000000.0 / (data[9][j][k]  * size3)
                            if len(data) > 10:
                                data[5][j][k] = data[10][j][k]* 1000000.0 / (data[11][j][k] * size3)
                                if len(data) > 12:
                                    data[6][j][k] = data[12][j][k]* 1000000.0 / (data[13][j][k] * size3)
                                    if len(data) > 14:
                                        data[7][j][k] = data[14][j][k]* 1000000.0 / (data[15][j][k] * size3)
                        
                label = "Sim time / DUT Time"
            elif metrics[0]=="sim_time":
                if plot_type==14:
                    plt.ylim([100, 1000000])
                    plt.yscale('log')
                else:
                    plt.ylim([0, 20000])
                label='Simulator Runtime (s)'
            elif metrics[0]=="arith_intensity_msgs":
                plt.ylim([0, 1])
                label='Arithmetic Intensity (FLOP/Msgs)'
            elif metrics[0]=="arith_intensity_loads":
                plt.ylim([0, 1])
                label='Arithmetic Intensity (FLOP/Loads)'
            elif plot_metric==6:
                plt.ylim([0, 1000])
                label='Throughput (FLOPs)'
            elif metrics[0]==perf_metric:
                if plot_type==19:
                    plt.ylim([0, 16])
                else:
                    plt.ylim([0, 5])
                label=perf_label+"/\$ Improvement"
                for k in range(0, len(inputs)):
                    data[0][j][k] = data[0][j][k]*1.0/data[1][j][k]
                    data[1][j][k] = data[2][j][k]*1.0/data[3][j][k]
                    data[2][j][k] = data[4][j][k]*1.0/data[5][j][k]
                    if len(data) > 6:
                        data[3][j][k] = data[6][j][k]*1.0/data[7][j][k]
                        if len(data) > 8:
                            data[4][j][k] = data[8][j][k]*1.0/data[9][j][k]
                            if len(data) > 10:
                                data[5][j][k] = data[10][j][k]*1.0/data[11][j][k]
                                if len(data) > 12:
                                    data[6][j][k] = data[12][j][k]*1.0/data[13][j][k]
            elif metrics[0]=="time":
                if plot_type==14: # Scaling Baseline vs Reduction
                    ax.set_yscale('log'); plt.ylim([0.7, 100])
                else:
                    plt.ylim([0, y_lim])
                label = perf_label+" Improvement"
            else:
                print("ERROR: Metric not supported: "+metrics[0])
                sys.exit(2)

        elif plot_type==1: #energy_breakdown
            plt.ylim([0, 100])
            label='Energy Breakdown (\% of total)'
            #colors=['yellow','r','b']
            for k in range(0, len(inputs)):
                total_energy = 0
                for i in range(0,plot_len):
                    total_energy += data[i][j][k]
                accum_energy.append(total_energy)

        elif plot_type==2:
            plt.ylim([0, y_lim])
            #j is app, k is dataset, i is binary
            for k in range(0, len(inputs)):
                cost_d1_2d = data[1][j][k] # DSIM-SRAM
                cost_d2_2d = data[5][j][k]
                cost_d3_25d = data[10][j][k]
                data[0][j][k] = data[0][j][k]*1.0/cost_d1_2d
                data[1][j][k] = data[4][j][k]*1.0/cost_d2_2d
                data[2][j][k] = data[8][j][k]*1.0/cost_d3_25d
                # check if data[12] is not out of bounds
                if len(data) > 12:
                    cost_3d_dram_config = data[11][j][k]
                    data[3][j][k] = data[12][j][k]*1.0/cost_3d_dram_config
                    #Check that HBM vertical and horizontal are the same
                    assert(data[12][j][k] == data[8][j][k])
        elif plot_type==3:
            plt.ylim([0, y_lim])
            for k in range(0, len(inputs)):
                for i in range(0,plot_len*5):
                    print(data[i][j][k])
                #["energy","3d_energy","cost_2d","cost_2.5d","cost_3d"]
                cost_d1_2d  = 1 #data[2][j][k]
                cost_d2_2d  = 1 #data[7][j][k]
                cost_25d_dram_config = 1 #data[13][j][k]
                cost_3d_dram_config  = 1 #data[14][j][k]
                data[0][j][k] = 1.0*pow(10,9)/data[0][j][k]/cost_d1_2d
                data[1][j][k] = 1.0*pow(10,9)/data[5][j][k]/cost_d2_2d
                data[2][j][k] = 1.0*pow(10,9)/data[10][j][k]/cost_25d_dram_config
                if len(data) > 16:
                    data[3][j][k] = 1.0*pow(10,9)/data[16][j][k]/cost_3d_dram_config
                    #Check that HBM vertical and horizontal are the same
                    assert(data[14][j][k] == data[19][j][k])

        elif plot_type==4 or plot_type==6: #TEPS, gigaoperations/s
            ax.set_yscale('log')
            label=''
            if metrics[2] == 'effective_bw': #absolute perf plot
                if (plot_type==4):
                    plt.ylim([1000000000, 100000000000000])
                else:
                    plt.ylim([1000000000, 1000000000000000])
            elif metrics[2] == 'cost_2.5d': #cost and TEPS/Watt plots
                if (plot_type==4 and plot_metric==1):
                    plt.ylim([5000000, 1000000000000])
                elif (plot_type==4):
                    plt.ylim([5000000, 10000000000])
                else:
                    plt.ylim([10000000, 10000000000])
                #colors=['red','b','r','m','y','k','w']
                for k in range(0, len(inputs)):
                    power = data[1][j][k] / 1000 # convert from miliwatts to watts
                    cost = data[2][j][k] #in dollars
                    total_msg = data[3][j][k]
                    gflops = data[0][j][k]
                    operations = gflops * 1000 * 1000 * 1000 #convert from gigaoperations/s to operations/s
                    data[0][j][k] = operations / cost  #operations/$
                    data[1][j][k] = operations / power #operations/s/Watt
                    data[2][j][k] = total_msg
            elif plot_metric==2:
                plt.ylim([100000000, 100000000000])
                for k in range(0, len(inputs)):
                    power = data[2][j][k] / 1000 # convert from miliwatts to watts
                    operations = data[1][j][k] * 1000 * 1000 * 1000
                    teps = data[0][j][k] * 1000 * 1000 * 1000 #convert from gigaoperations/s to operations/s
                    data[0][j][k] = teps / power  #Teps/Watt
                    data[1][j][k] = operations / power #operations/s/Watt
                    data[2][j][k] = 1
            elif plot_metric==3:
                if apps[len(apps)-1] == 'fft':
                    plt.ylim([1, 10000000000000])
                else:
                    plt.ylim([1000, 5000000000])
                size1 = 1024
                for k in range(0, len(inputs)):
                    sim_time = data[0][j][k]
                    #dut_time = data[1][j][k]
                    operations = data[2][j][k] * 1000 * 1000 * 1000
                    total_msg = data[3][j][k]
                    data[0][j][k] = sim_time #* 1000000.0 / (dut_time * size1)
                    data[1][j][k] = operations / sim_time
                    data[2][j][k] = total_msg / sim_time
                    size1 *= 4

        elif plot_type==5: #Tca comparison
            if metrics[0]=="edges":
                plt.ylim([0, 2])
            elif metrics[0]=="t3_hops":
                plt.ylim([0, 30])
                for k in range(0, len(inputs)):
                    for i in range(0,plot_len*2):
                        print(data[i][j][k])
                    data[1][j][k] = data[2][j][k]
                    data[2][j][k] = data[4][j][k]
                    data[3][j][k] = data[6][j][k]
                    data[4][j][k] = data[9][j][k]
                    data[5][j][k] = data[11][j][k]
                    data[6][j][k] = data[13][j][k]
                    data[7][j][k] = data[15][j][k]
            else:
                ax.set_yscale('log')
                plt.ylim([0.2, 1000])
                

        # Exponent in bars footer
        objs=[]
        if plot_type==9:
            global font_xticks; font_xticks = 1
            for inpt in inputs:
                objs.append("")

        elif plot_type==17:
            for inpt in inputs:
                grid_x = int(inpt.split("--")[1])
                objs.append("%dx%d" % (grid_x,grid_x))
        elif plot_type==4 or plot_type==6:
            count = 0
            for inpt in inputs:
                grid_x = int(inpt)
                exp = np.log2(grid_x)*2
                if count%2==0:
                    objs.append("$2^{%d}$" % exp)
                else:
                    objs.append("")
                count=count+1
        else:
            # Based on the keys
            keys = list(leg_inputs.keys())
            for inpt in inputs:
                if inpt in keys:
                    objs.append(leg_inputs[inpt])
                elif "--" in inpt:                 
                    strings = inpt.split("--")
                    objs.append("%s" % (strings[1]))
                else:
                    objs.append("")

        y_pos = np.arange(len(objs))
        if bars==1:
            bars+=1
        bar_width = 0.9/(bars-1)

        print("\ngeomean improvement app %s" % apps[j])
        
        data_per_app = []
        if plot_type==2 or plot_type==3:
            normalized = data[0][j]
        else:
            normalized = data[0][j]
        print("Normalized: "+str(normalized))

        for i in range(0,plot_len):
            #i = plot_len - 1 - count
            print("Binary %d" % i)
            if metrics[0]=="t3_hops" or metrics[0]=="Core_active":
                print(data[i][j])
            elif metrics[0]=="router_active" or metrics[0]=="dhit_rate":
                print(data[i][j])
            elif metrics[0]=="sim_time" or metrics[0]=="arith_intensity_msgs" or metrics[0]=="arith_intensity_loads" or plot_metric==6:
                data[i][j] = [ 1.0*x for x in data[i][j]]
                print(data[i][j])
            elif plot_type==1: #energy percentage
                data[i][j] = [ (x*1.0/y)*100.0 for x,y in zip(data[i][j], accum_energy)]
            elif plot_type==18: #fft validation
                ref_fft = [[13633], [32176], [82405], [236329], [815371]]
                print("Sim time: "+str(data[i][j]))
                data[i][j] = [ (y*0.001/x) for x,y in zip(data[i][j], ref_fft[i])]
                print("Ratio: "+str(data[i][j]))
            elif (plot_type<=3) or plot_metric==10: #perf/$ or energy_efficiency. The Higher the better!
                data[i][j] = [ (x*1.0/y) for x,y in zip(data[i][j], normalized)]
                print(data[i][j])
            elif metrics[0]=="time" or metrics[0]=="energy": # the lower the better for all these!
                data[i][j] = [ (y*1.0/x) for x,y in zip(data[i][j], normalized)]
                print(data[i][j])
            elif metrics[0]=="total_msg":
                if plot_type==14:
                    data[i][j] = [ (x*1.0/y) for x,y in zip(data[i][j], normalized)]
                else:
                    data[i][j] = [ (y*1.0/x) for x,y in zip(data[i][j], normalized)]
                print(data[i][j])
            else:
                print("Absolute value")

            geomean = geo_mean(data[i][j])
            data_per_app.append(geomean)
            print("over %d: %0.10f" % (i,geomean) )

            if (plot_type==4 or plot_type==6):
                if metrics[2]=="cost_2.5d" or plot_metric>=2: # raw data
                    data[i][j] = [ 1.0*x for x in data[i][j]]
                else: #operations/Watt
                    data[i][j] = [ x*pow(10,9) for x in data[i][j]]

        geomean_per_app.append(data_per_app)
        geomean_per_type = list(map(list, zip(*geomean_per_app)))
        print(geomean_per_type)

        # legend box
        do_not_show_legend = ((plot_type >= 24 and plot_type <= 27) and plot_metric!=7) or ((plot_type==11 or plot_type==12 or plot_type==21) and plot_metric!=8) or (plot_type==14 and plot_metric==0) or ((plot_type==9 or plot_type==19) and plot_metric!=10) or ((plot_type==20 or plot_type==23) and plot_metric!=7) or (plot_type==1 and plot_metric==0) or (plot_type==2)

        line_plot = (plot_type==14) or (plot_type==4) or (plot_type==6)
        if not line_plot:
            rects_list=[]
            for i in range(0,plot_len):
                data_for_bar = data[i][j]
                bar_id = i; zonder_id = i+1
                if plot_type==12:
                    if i>3:
                        bar_id=i-2
                        zonder_id=0
                        if i==4:
                            colors[i]='yellow'
                            hatches[i]=hatches[i-2]
                        elif i==5:
                            colors[i]='lightpink'
                            hatches[i]=hatches[i-2]
                barr = plt.bar(y_pos+bar_id*bar_width, data_for_bar, bar_width, color=colors[i], alpha=1.0, linewidth=0.1, zorder=zonder_id,hatch=hatches[i])
                rects_list.append(barr)
            plt.xticks(y_pos+0.3, objs, fontsize=font_xticks, rotation=0)
            if j==0 and not do_not_show_legend:
                plt.legend(rects_list, comp, handletextpad=0.15, fontsize=font, bbox_to_anchor=(0, legend_top_sep, legend_width, legend_height), loc=1, ncol=cols, mode="expand", borderaxespad=0.)
            if plot_metric!=7 and plot_type!=3 and plot_type!=2 and plot_metric!=11: #not log plots, not small space within bars as in energy plots
                ax.yaxis.set_minor_locator(AutoMinorLocator())
        else:
            if plot_type==14:
                #bar_width = 0.9 / (plot_len-1)
                x_positions = [y_pos + i * bar_width for i in range(plot_len)]
                line_x_pos = [x_positions[0], x_positions[2], x_positions[4]]
                line1 = plt.plot(line_x_pos, [data[0][j], data[2][j], data[4][j]], marker='o', linestyle='-', color='blue')
                line2 = plt.plot(line_x_pos, [data[1][j], data[3][j], data[5][j]], marker='*', linestyle='-', color='darkorange')
                plt.xticks(y_pos+0.3, objs, fontsize=font_xticks, rotation=0)
                if j==0 and not do_not_show_legend:
                    line_for_legend.append(line1[0]); line_for_legend.append(line2[0])
                    legend_labels.append("Baseline"); legend_labels.append("Reduction")
                    plt.legend(line_for_legend, legend_labels, ncol=len(legend_labels), handletextpad=0.15, fontsize=font, bbox_to_anchor=(1, legend_top_sep, 4, .08), loc=1, mode="expand", borderaxespad=0.)
                    
            elif plot_type==4 or plot_type==6:
                line1 = plt.plot(y_pos, data[0][j], marker='o', linestyle='-', color='blue')
                line2 = plt.plot(y_pos, data[1][j], marker='*', linestyle='-', color='red')
                if plot_type==4 or metrics[2]=="effective_bw" or plot_metric==3:
                    line3 = plt.plot(y_pos, data[2][j], marker='x', linestyle='-', color='green')

                plt.xticks(y_pos, objs, fontsize=font_xticks, rotation=0)
                if j==0:
                    line_for_legend.append(line1[0]); line_for_legend.append(line2[0])
                    if metrics[2]=="effective_bw":
                        line_for_legend.append(line3[0])
                        legend_labels.append(perf_word); legend_labels.append("Operations/s"); legend_labels.append("Avg. Mem. BW (Bytes/s)")
                    elif metrics[2]=="cost_2.5d":
                        legend_labels.append(perf_word+"/\$"); legend_labels.append(perf_word+"/Watt")
                        if plot_type==4:
                            line_for_legend.append(line3[0])
                            legend_labels.append("NoC Message Hops")
                    elif plot_metric==2:
                        legend_labels.append(perf_word+"/Watt"); legend_labels.append("Operations/s/Watt")
                    elif plot_metric==3:
                        line_for_legend.append(line3[0])
                        legend_labels.append("Sim Time (s)"); legend_labels.append("Ops/s"); legend_labels.append("Msg/s")
                    plt.legend(line_for_legend, legend_labels, ncol=len(legend_labels), handletextpad=0.15, fontsize=font, bbox_to_anchor=(legend_left_point, legend_top_sep, legend_width, .08), loc=1, mode="expand", borderaxespad=0.)

            # Adjust X ticks width
            xlims = ax.get_xlim()
            padding = (xlims[1] - xlims[0]) * -0.05 # Adjust the padding percentage as needed
            new_xlims = (xlims[0] + padding, xlims[1] - padding)
            ax.set_xlim(new_xlims)
        
        plt.grid(True, which='major', axis = 'y', color = 'black', linestyle = '--', linewidth = 0.15, zorder=0)
        plt.grid(True, which='minor', axis = 'y', color = 'gray', linestyle = '--', linewidth = 0.03, zorder=1)
        #get y lim from plt
        ylims = ax.get_ylim()
        ylim_scale = ax.get_yscale()
        ax.set_axisbelow(True)

        title = apps[j].upper()
        if title=="PAGERANK": title="PAGE"

        plt.title(title, fontsize=font)  
        if j==0:
            plt.ylabel(label, fontsize=font)
            plt.yticks(fontsize=font-4)
        else:
            plt.yticks(fontsize=0)

        ax.yaxis.get_offset_text().set_size(font-2)
        ax.yaxis.set_ticks_position('left')
        ax.tick_params(axis='y', which='major', pad=0)
    
    plot_geo = plot_type!=4 and plot_type!=6
    if plot_geo:
        ax = plt.subplot(gs[0, len(apps) * app_cols: len(apps) * app_cols + 1],frameon=False)  # Full width
        plt.grid(True, which='major', axis = 'y', color = 'black', linestyle = '--', linewidth = 0.15, zorder=0)
        plt.grid(True, which='minor', axis = 'y', color = 'gray', linestyle = '--', linewidth = 0.03, zorder=1)
        plt.rcParams["font.family"] = "Times New Roman"
        plt.ylim(ylims)
        ax.set_yscale(ylim_scale)
        plt.xticks([1], [''], fontsize=font_xticks, rotation=0)
        plt.yticks(fontsize=0)
        if plot_metric!=7 and plot_type!=14 and plot_type!=3 and plot_type!=2 and plot_metric!=11:
            ax.yaxis.set_minor_locator(AutoMinorLocator())
        plt.title("Geo", fontsize=font)  

    for i in range(0,plot_len):
        geo_data = geo_mean(geomean_per_type[i])
        if plot_geo and not line_plot:
            bar_id = i; zonder_id = i+2
            if plot_type==12:
                if i>3:
                    bar_id=i-2
                    zonder_id=0
                    if i==4:
                        colors[i]='yellow' #'mediumseagreen' #'lightgreen'
                        hatches[i]=hatches[i-2]
                    elif i==5:
                        colors[i]='lightpink' #'orchid' #'lightpink'
                        hatches[i]=hatches[i-2]
                comp = ("NoProxy","Proxy32x32","Proxy16x16F", "Proxy8x8F","Proxy16x16","Proxy8x8", )

            print("total geomean of %s %0.2f" % (comp[i], geo_data ) )

            barr = plt.bar(bar_id*bar_width, geo_data, bar_width, color=colors[i], alpha=1.0, linewidth=0.1, zorder=zonder_id,hatch=hatches[i])

    if plot_geo and line_plot:
        if plot_type==14:
            x_positions = [i * bar_width for i in range(plot_len)]
            line_x_pos = [x_positions[0], x_positions[2], x_positions[4]]
            line1 = plt.plot(line_x_pos, [geo_mean(geomean_per_type[0]), geo_mean(geomean_per_type[2]), geo_mean(geomean_per_type[4]) ], marker='o', linestyle='-', color='blue')
            line2 = plt.plot(line_x_pos, [geo_mean(geomean_per_type[1]), geo_mean(geomean_per_type[3]), geo_mean(geomean_per_type[5]) ], marker='*', linestyle='-', color='darkorange')


    for i in range(2,len(comp)):
        print("total geomean of %s over %s %0.2f" % (comp[i], comp[i-1], geo_mean(geomean_per_type[i])/geo_mean(geomean_per_type[i-1]) ) )
        print("total geomean of %s over %s %0.2f" % (comp[i], comp[i-2], geo_mean(geomean_per_type[i])/geo_mean(geomean_per_type[i-2]) ) )


    plt.savefig('plots/characterization%d_%d.pdf' %  (plot_type, plot_metric), bbox_inches='tight')

if __name__ == "__main__":
   main(sys.argv[1:])