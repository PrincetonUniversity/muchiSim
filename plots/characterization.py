#!/usr/bin/python

from cmath import log
import matplotlib as mpl
from parse import *
mpl.use('pdf')

import matplotlib.pyplot as plt
from matplotlib import rc
import math
import numpy as np
import sys, getopt
plt.rcParams["font.family"] = "Times New Roman"
rc('font',**{'family':'serif','serif':['Times New Roman']})
rc('text', usetex=True)

folder = ["dlxsim"]
font = 15
leg_inputs = {'ego-Twitter':'TT', 'liveJournal':'LJ', 'amazon-2003':'AZ', 'wikipedia':'WK', 'Kron16':'R16', 'Kron22':'R22', 'Kron25':'R25', 'Kron26':'R26'}

default_grid_width = 64

def geo_mean(iterable):
    a = np.array(iterable)
    return a.prod()**(1.0/len(a))

def collect_data(binaries, metrics, apps, inputs,plot_type):
    data_planes = []
    data_metric = []
    for metric_name in metrics:
        pattern_index = metric_of_interest[int(metrics_names[metric_name])]
        data_apps = []
        for app in apps:
            print(app)
            data_dataset = []
            for dataset in inputs:
                data_bin = []
                binaries_p = binaries

                for binary in binaries_p:
                    keys = list(leg_inputs.keys())
                    #Support for plot increasing grid sizes
                    if dataset not in keys:
                        grid = int(dataset)
                        dataset="Kron26"
                    else:
                        grid = default_grid_width

                    print(binary)
                    # Split binary into type and grid config
                    if "--" in binary:
                        strings = binary.split("--")
                        binary=strings[0]
                        grid=int(strings[1])
                        if (plot_type==14 or plot_type==1) and (dataset=="Kron26"):
                            grid=grid*2
                        if (dataset=="Kron26") and (binary=="L1" or binary=="L2"):
                            binary="SCALING"

                    value = -1
                    if grid==1024:
                        f = 1
                    else:
                        f = 0
                    while (value == -1) and f<len(folder):
                        found,filename,temp = search(folder[f],dataset, grid, binary,app) #binary.replace('P','K')
                        f = f+1
                        if found==1:
                            value = read_file(filename,temp,pattern_index)
                        else:
                            value = -1
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
                    output.append(int(data))
                a=a+1
                data_from_this_binary.append(output)
            data_planes.append(data_from_this_binary)
        b=b+1
    return data_planes

def main(argv):
    usage = "characterization.py -p <plot_type> (0:runtime, 1:energy_breakdown, 2:hmc_time, 3:hmc_energy) -m <metric> (0:time, 1:utilization)"
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

    if (plot_type>=10):
        apps = ['sssp','pagerank','bfs', 'wcc','spmv','histo']
        #Default values for Plot types > 10
        y_lim = 10
        inputs = ['wikipedia','Kron22']
        left_corner = -5
        legend_width = 6

        if plot_type==10: #QUEUE size comparison
            y_lim = 5
            binaries = ["QUEUE1","QUEUE3","QUEUE4","QUEUE5","QUEUE6"]
            comp = ("IQ=OQ","IQ=OQx4","IQ=OQx8","IQ=OQx16","IQ=OQx32")

        elif plot_type==11: #LOOP block comparison (dalang experiment)
            binaries = ["I16L1","I16L2","I16L3","I16L4","I16L5","I16L6"]
            comp = ("1","4","8","16","32","64")

        elif plot_type==12: #PROXY size comparison
            y_lim = 5
            binaries = ["PROXY64","PROXY32","PROXY16","PROXY8"]
            comp = ("NoProxy (Dlx)","Proxy32x32","Proxy16x16","Proxy8x8")

        elif plot_type==13: #STEAL REGION comparison
            binaries = ["S16S0X0","S16S0X1","S16S1X0","S16S1X1"]
            comp = ("S16S0X0","S16S0X1","S16S1X0","S16S1X1")

        elif plot_type==14: #Dalorex vs Tascade
            inputs = ['Kron25','Kron26']
            binaries = ["L0--64","L1--64","L0--128","L1--128","L0--256","L1--256"]
            comp = ("Dalorex-1","Tascade-1","Dalorex-2","Tascade-2","Dalorex-3","Tascade-3")
        elif plot_type==15: #NETWORK
            y_lim = 3
            binaries = ["NOCN0","NOCN1","NOCN2","NOCN3"]
            comp = ("ID:32,32, D2D:32", "ID:32,64, D2D:32", "ID:32,64, D2D:32,32", "ID:32,64, D2D:32,64")
        elif plot_type==16: #PREFETCHING
            y_lim = 4
            binaries = ["DMBC6","DMBC5","DMBC4","DMBC3","DMBC1"]
            comp = ("Always HBM","Cache last request","Remote SRAM","Local SRAM","Always SRAM")
            legend_width = 3.2
            cols=3
            left_corner = -(legend_width/2)+0.3


        #whether we show utilization or normalized performance
        if plot_metric==0:
            metrics = ["time"]
        elif plot_metric==1:
            metrics = ["Core_active"]
        elif plot_metric==2:
            metrics = ["router_energy"]

        plot_len = len(binaries)
        colors=['yellow','gold','red','deepskyblue','lightblue','darkviolet','navy','green','lime']
        bars = len(inputs)+len(comp)/1.1

    elif (plot_type==1): #ENERGY BREAKDOWN
        if plot_metric==0:
            binaries = ["L1--64"]
        else:
            binaries = ["L2--16"]
        metrics = ["core_energy","mem_energy","router_energy"]
        plot_len = len(metrics)
        apps = ['sssp','bfs', 'wcc','spmv','histo']
        inputs = ['Kron25','Kron26']
        comp = ('Logic', 'Memory', 'Network')
        left_corner = -4
        legend_width = 4.8
        bars = len(inputs)+len(comp)/1.1

    # "PACKAGES" experiment, comparing Tascade-SRAM, Dalorex and Tascade-HBM
    elif (plot_type==2 or plot_type==3): #perf/$ or energy/op
        binaries = ["L0--128","L1--128","L2--32","L2--32"]
        plot_len = len(binaries)
        apps = ['sssp','bfs','wcc','spmv','histo']
        inputs = ['Kron25','Kron26']
        comp = ('Dlx-Chiplet','Tca-SRAM','Tca-HBM-Horiz.','Tca-HBM-Vert.')
        legend_width = 5
        left_corner = -4
        colors=['yellow','gold','lightcoral','red','lightblue','deepskyblue','darkviolet','navy']
        bars = len(inputs)+len(comp)/1.1
        if plot_type==2:
            y_lim = 5
            label='TEPS/\$'
            metrics = ["TEPS","cost_2d","cost_2.5d","cost_3d"] #["time"]
        else:
            y_lim = 5
            label='Ops/J/\$'
            metrics = ["energy","3d_energy","cost_2d","cost_2.5d","cost_3d"]

    elif (plot_type==4): #MASSIVE SCALING
        binaries = ["SCALING"]
        apps = ['sssp','bfs', 'wcc','spmv','histo']
        inputs = ['16','32','64','128','256','512','1024']

        if plot_metric==0:
            metrics = ["TEPS","gops","effective_bw"]
            comp = ["TEPS", "Operations/s", "Avg. Mem. BW (Bytes/s)"]
        else:
            metrics = ["gops","power","cost_2d"]
            comp = ["Operations/s/W", "Operations/s/\$"]

        plot_len = len(metrics)
        legend_width = 5
        left_corner = -4
        bars = len(inputs)/2+len(binaries)

    elif (plot_type==5): #Tca
        cfgs=[16,32,64,128]
        bin="01"
        tascade="Tca"
        binaries = [bin+"--"+str(cfgs[0]), bin+"--"+str(cfgs[1]),  bin+"--"+str(cfgs[2]), bin+"--"+str(cfgs[3]), tascade+"--"+str(cfgs[0]), tascade+"--"+str(cfgs[1]),  tascade+"--"+str(cfgs[2]), tascade+"--"+str(cfgs[3])]

        metrics = ["edges_cycle"]
        plot_len = len(binaries)
        apps = ['sssp','pagerank','bfs', 'wcc','spmv']
        inputs = ['Kron26']
        if len(binaries)==4:
            comp = ('Dlx-'+str(cfgs[0]**2),'Dlx-'+str(cfgs[1]**2),'Dlx-'+str(cfgs[2]**2),'Dlx-'+str(cfgs[3]**2) )
            colors=['lightcoral','darkorange','lightblue','navy']
            cols=2
        if len(binaries)==6:
            comp = ('Dalorex-64','Dalorex-256','Dalorex-1024','Tca-64','Tca-256','Tca-1024')
            colors=['lightcoral','darkorange','r','lightblue','deepskyblue','navy']
            cols=3
        elif len(binaries)==8:
            comp = ('Dalorex-16x16','Dalorex-32x32','Dalorex-64x64','Dalorex-128x128','Tascade-16x16','Tascade-32x32','Tascade-64x64','Tascade-128x128')
            colors=['lightcoral','darkorange','r','maroon','lightblue','slateblue','deepskyblue','navy']
            cols=4
        elif len(binaries)==10:
            comp = ('Dlx-16','Dlx-64','Dlx-256','Dlx-1024','Dlx-4096','Tca-16','Tca-64','Tca-256','Tca-1024','Tca-4096')
            colors=['yellow','lightcoral','darkorange','red','maroon','lightblue','slateblue','deepskyblue','navy','k']
            cols=5

        left_corner = -1.1 * (len(apps)-1)
        legend_width = 1.1 * len(apps)
        if metrics[0] =="time":
            label='Performance Improvement'
        elif metrics[0] =="edges_cycle":
            label='Throughput Improvement'
        elif metrics[0] =="t3_hops":
            label='Avg. Number of Router hops (T3)'
        #https://matplotlib.org/stable/gallery/color/named_colors.html
        bars = len(inputs)+len(binaries)-2

    data = collect_data(binaries,metrics,apps,inputs,plot_type)
    geomean_per_app = []

    width = max(2*len(apps), 2*len(inputs))
    plt.figure(figsize=(width,3.5)) #size of the global figure
    plt.subplots_adjust(left=None, bottom=None, right=None, top=None, wspace=0.02, hspace=0.0000001)
    
    for j in range(len(apps)):
        accum_energy = []
        ax = plt.subplot(1, len(apps), j+1,frameon=False)
        plt.rcParams["font.family"] = "Times New Roman"
 
        if plot_type==0: #network comparison
            plt.ylim([0, 3])
            label='Performance Improvement'
        elif plot_type>=10: #queue comparison
            if metrics[0]=="Core_active":
                plt.ylim([0, 100])
                label='PU Utilization'
            else: #performance
                plt.ylim([0, y_lim])
                label='Performance Improvement'

        elif plot_type==1: #energy_breakdown
            plt.ylim([0, 100])
            label='Energy Breakdown (\% of total)'
            colors=['yellow','r','b']
            for k in range(0, len(inputs)):
                total_energy = 0
                for i in range(0,plot_len):
                    total_energy += data[i][j][k]
                accum_energy.append(total_energy)

        elif plot_type==2:
            #ax.set_yscale('log')
            #plt.ylim([0, 1000])
            plt.ylim([0, y_lim])
            #j is app, k is dataset, i is binary
            for k in range(0, len(inputs)):
                for i in range(0,plot_len*4):
                    print(data[i][j][k])
                cost_2d_sram_config = data[1][j][k]
                cost_25d_dram_config = data[10][j][k]
                if data[4][j][k] < data[0][j][k]:
                     data[4][j][k] = data[0][j][k]
                data[0][j][k] = data[0][j][k]*1.0/cost_2d_sram_config
                data[1][j][k] = data[4][j][k]*1.0/cost_2d_sram_config
                data[2][j][k] = data[8][j][k]*1.0/cost_25d_dram_config
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
                cost_2d_sram_config = data[2][j][k]
                cost_25d_dram_config = data[13][j][k]
                cost_3d_dram_config = data[14][j][k]
                # if data[4][j][k] > data[0][j][k]:
                #      data[4][j][k] = data[0][j][k]
                data[0][j][k] = 1.0*pow(10,9)/data[0][j][k]/cost_2d_sram_config
                data[1][j][k] = 1.0*pow(10,9)/data[5][j][k]/cost_2d_sram_config
                data[2][j][k] = 1.0*pow(10,9)/data[10][j][k]/cost_25d_dram_config
                if len(data) > 16:
                    data[3][j][k] = 1.0*pow(10,9)/data[16][j][k]/cost_3d_dram_config
                    #Check that HBM vertical and horizontal are the same
                    assert(data[14][j][k] == data[19][j][k])

        elif plot_type==4: #edges/s, gops
            ax.set_yscale('log')
            if metrics[0] == 'TEPS':
                plt.ylim([1000000000, 1000000000000000])
            elif metrics[0] == 'gops':
                plt.ylim([100000000, 100000000000])

            if metrics[0]=="TEPS":
                label='' #'Scaling RMAT-26 (legend units)'
                colors=['yellow','r','b','m','y','k','w']
            elif metrics[0]=="gops":
                label='' #Scaling RMAT-26 (legend units)'
                colors=['red','b','r','m','y','k','w']
                for k in range(0, 7):
                    power = data[1][j][k] / 1000
                    cost = data[2][j][k]
                    gops = data[0][j][k] * 1000 * 1000 * 1000
                    data[0][j][k] = gops / cost
                    data[1][j][k] = gops / power
                    data[2][j][k] = 1

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
        if plot_type==4:
            for inpt in inputs:
                exp = int(inpt)
                exp = np.log2(exp)*2
                objs.append("$2^{%d}$" % exp)
        elif plot_type<16:
            for inpt in inputs: objs.append(leg_inputs[inpt])
        else:
            objs.append("")

        y_pos = np.arange(len(objs))
        if bars==1:
            bars+=1
        bar_width = 0.9/(bars-1)

        print("\ngeomean improvement app %s" % apps[j])
        
        data_per_app = []
        normalized = data[0][j]
        for i in range(0,plot_len):
            #i = plot_len - 1 - count
            print("Binary %d" % i)
            print(normalized)
            if metrics[0]=="t3_hops" or metrics[0]=="Core_active":
                print(data[i][j])
            elif metrics[0]=="router_active":
                print(data[i][j])
            elif plot_type==1: #energy
                data[i][j] = [ (x*1.0/y)*100.0 for x,y in zip(data[i][j], accum_energy)]
            elif (plot_type>5): #time
                data[i][j] = [ (y*1.0/x) for x,y in zip(data[i][j], normalized)]
                print(data[i][j])
            elif (plot_type<=3): #perf/$ or energy_efficiency/$
                data[i][j] = [ (x*1.0/y) for x,y in zip(data[i][j], normalized)]
                print(data[i][j])
            elif plot_type==5: #t3 hops
                if metrics[0]=="time":
                    data[i][j] = [ (y*1.0/x) for x,y in zip(data[i][j], normalized)]
                else:
                    data[i][j] = [ (x*1.0/y) for x,y in zip(data[i][j], normalized)]
                
                print(data[i][j])
            geomean = geo_mean(data[i][j])

            if (plot_type==4):
                if metrics[0]=="gops":
                     data[i][j] = [ 1.0*x for x in data[i][j]]
                else: #metric is TEPS
                    data[i][j] = [ x*pow(10,9) for x in data[i][j]]
            data_per_app.append(geomean)
            print("over %d: %0.10f" % (i,geomean) )
           
        geomean_per_app.append(data_per_app)

        rects_list=[]
        for i in range(0,plot_len):
            data_for_bar = data[i][j]
            barr = plt.bar(y_pos+i*bar_width, data_for_bar, bar_width, color=colors[i], alpha=1.0, linewidth=0.1, zorder=i+1)
            rects_list.append(barr)

        plt.grid(True, which='major', axis = 'y', color = 'black', linestyle = '--', linewidth = 0.15, zorder=0)
        plt.grid(True, which='minor', axis = 'y', color = 'gray', linestyle = '--', linewidth = 0.03, zorder=1)
        ax.set_axisbelow(True)

        title = apps[j].upper()
        if title=="PAGERANK": title="PageRank"

        plt.title(title, fontsize=font)  
        plt.xticks(y_pos+0.3, objs, fontsize=font-1, rotation=0)
        if j==0:
            plt.ylabel(label, fontsize=font)
            plt.yticks(fontsize=font-4)
        else:
            plt.yticks(fontsize=0)

        ax.yaxis.get_offset_text().set_size(font-2)
        ax.yaxis.set_ticks_position('left')
        ax.tick_params(axis='y', which='major', pad=0)

    geomean_per_type = list(map(list, zip(*geomean_per_app)))
    print(geomean_per_type)

    for i in range(0,len(comp)):
        print("total geomean of %s %0.2f" % (comp[i], geo_mean(geomean_per_type[i]) ) )
    for i in range(2,len(comp)):
        print("total geomean of %s over %s %0.2f" % (comp[i], comp[i-1], geo_mean(geomean_per_type[i])/geo_mean(geomean_per_type[i-1]) ) )
        print("total geomean of %s over %s %0.2f" % (comp[i], comp[i-2], geo_mean(geomean_per_type[i])/geo_mean(geomean_per_type[i-2]) ) )

       
    plt.rcParams['pdf.fonttype'] = 42

    # legend box
    vertical_separation = -0.23
    plt.legend(rects_list, comp, handletextpad=0.15, fontsize=font, bbox_to_anchor=(left_corner, vertical_separation, legend_width, .08), loc=1, ncol=cols, mode="expand", borderaxespad=0.)
    plt.savefig('plots/characterization%d_%d.pdf' %  (plot_type, plot_metric), bbox_inches='tight')

if __name__ == "__main__":
   main(sys.argv[1:])