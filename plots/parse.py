#!/bin/bash
import os,time
import re, sys
import matplotlib as mpl
from matplotlib import rc
mpl.use('pdf')
import matplotlib.pyplot as plt
import numpy as np
plt.rcParams["font.family"] = "Times New Roman"
rc('font',**{'family':'serif','serif':['Times New Roman']})
rc('text', usetex=True)

override_temp = True
metric_of_interest = [2, 9, 17, 18, 1, 6, 7, 8, 20, 21, 22,23,5,24,15,16,25,26,27,28,29,30,31,32,0,3]
metrics_names = {'energy':'0', 'time':'1', 'gops':'2', 'effective_bw':'3', 'dynamic_nj':'4', 'area_mib':'5', 'nodes':'6', 'edges':'7',
                 'core_energy':'8', 'router_energy':'9', 'mem_energy':'10',"total_area":"11","kib_per_core":"12", "T3_invocations":"13",
                 "nodes_cycle":"14","edges_cycle":"15","t3_hops":"16","cost_2d":"17","cost_2.5d":"18","cost_3d":"19","TEPS":"20","NOC3_lat":"21","Core_active":"22","t3b_hops":"23", "3d_energy" : "24", "power" : "25"}

patterns = [
    #"Leakage Energy \(nJ\)",
    "Total Energy \(nJ\) 3D", #0
    "Dynamic Energy \(nJ\)", #1
    "Total Energy \(nJ\)", #2
    "Avg. Power \(mW\)", #3
    "Power Density \(mW/mm2\)",
    "Storage per Core", #deprecated
    "Total Storage", #6, deprecated
    "Number of K Nodes Processed", #7
    "Number of K Edges Processed", #8
    "Runtime in K cycles", #9
    "Miliseconds \(at 1.00 Ghz\)",
    "K Loads",
    "K Stores",
    "K Instructions",
    "K Messages",
    "Nodes/cy", #15
    "Edges/cy", #16
    "GOP/s \(aka OPS/ns\)", #17
    "Avg. BW GB/s", #18
    "Max BW GB/s",
    "Cores Energy \(nJ\)", #20
    "Route Energy \(nJ\)", #21
    "Mem Energy   \(nJ\)", #22,
    "Area used \(mm2\)", #23
    "T3 invocations", #24
    "Avg. hops NOC2", #25
    "System Cost 2d", #26
    "System Cost 2.5d", #27
    "System Cost 3d", #28
    "TEPS", #29
    "Avg. latency NOC3 \(T3b\)", #30
    "Core Active Avg", #31
    "Avg. hops NOC3", #32
    ]
    
keys = {'bfs':'2', 'wcc':'3', 'pagerank':'1', 'sssp':'0', 'spmv':'4', 'histo':'5', 'multi':'6'}

def search(folder, dataset, mesh, binary,app):
    found = 0
    temp = "plots/temp/TEMP-%s--%s-X-%s--B%s-A%s.log" % (dataset, mesh, mesh, binary, keys[app])
    filename = "%s/DATA-%s--%s-X-%s--B%s-A%s.log" % (folder,dataset, mesh, mesh, binary, keys[app])
    if not os.path.exists(temp) or override_temp:
        if os.path.exists(filename):
            found=1
            cmd = "grep -v -e cache_sets_log2 -e local_address_space_lo %s 2>/dev/null | head -n %d > %s" % (filename, 135,temp)
            os.system(cmd)
            cmd = "tail -n %d %s >> %s" % (300, filename,temp)
            os.system(cmd)
        # else:
        #     print("%s FILE NOT FOUND! IN FOLDER: %s\n" % (filename, folder) )
    else:
        found=1
    return found,filename,temp

def read_file(filename,temp,pattern_index):
    # pattern match a number with thousands separator
    #pattern = re.compile(r'(\d{1,3}(?:,\d{3})*|\d+)(?:\.\d+)?')

    numerical = -1
    pat = patterns[pattern_index]
    if pattern_index == 31:
        pattern = re.compile(r"(%s:)(\d+)" % pat)
    else:
        pattern = re.compile(r"(%s:)\s(.*)" % pat)
    if not os.path.exists(filename):
        print("%s NOT FOUND!\n" % filename)
        print("ERROR: FILE NOT FOUND")
        sys.exit(2)
    else:
        with open(temp, "r") as file:
            #print("%s FOUND\n" % filename)
            lines = file.readlines()
            finished = 0
            last_line = lines[-1] + lines[-2] + lines[-3]
            # print (last_line)
            #say whether last_line contains version
            if "Version" in last_line:
                finished = 1
            for line in reversed(lines):
                value = re.search(pattern, line)
                if value:
                    num_with_commas = value.group(2)
                    numerical = float(num_with_commas.replace(',','').replace('$',''))
                    if finished == 0:
                        print("\n===\nWARNING: %s NOT FINISHED\n===\n" % filename)
                        os.system("ls -l %s" % filename)
                    return numerical
    
    print("VALUE NOT FOUND FOR %s" % pat)            
    return -1