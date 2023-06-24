import matplotlib as mpl
mpl.use('pdf')
import matplotlib.pyplot as plt
from matplotlib import rc
import numpy as np
import os
import re,time
import sys, getopt

font_size = 24
plt.rcParams["font.family"] = "Times New Roman"
plt.rcParams["font.size"] = font_size
rc('font',**{'family':'serif','serif':['Times New Roman']})
rc('text', usetex=True)

metric_keys = {'want_routers':'0', 'PU':'1', 'Router' : '0'}
keys = {'bfs':'2', 'wcc':'3', 'pagerank':'1', 'sssp':'0','spmv':'4'}
replace_temp=True

def plot(app,dataset,mesh,metric,binary,lower_bound):
    folder = "dlxsim"
    temp = "plots/temp/TEMP-%s--%s-X-%s--B%s-A%s.log" % (dataset, mesh, mesh, binary, keys[app])
    filename = "%s/DATA-%s--%s-X-%s--B%s-A%s.log" % (folder,dataset, mesh, mesh, binary, keys[app])

    if not os.path.exists(temp) or replace_temp:
        print("%s not found\n" % temp)
        if not os.path.exists(filename):
            print("%s not found\n" % filename)
            exit()
        cmd = "tail -n %d %s > %s" % (150+ mesh*8 + mesh*mesh, filename,temp)
        print(cmd)
        os.system(cmd)
    data = []
    for i in (range(0,mesh,1)):
        data_dim = []
        for j in (range(0,mesh,1)):
            with open(temp, "r") as file:
                #print("%s FOUND\n" % filename)
                lines = file.readlines()
                found = 0
                string_pat = r"X:%s\s*Y:%s\s*R:\s(\d+).*\s*C:\s(\d+).*MSG1:\s(\d+)" % (j,i)
                #print(string_pat)
                pattern = re.compile(string_pat)
                for line in lines:
                    value = re.search(pattern, line)
                    if value:
                        #print("value found ")
                        val = float(value.group(int(metric_keys[metric])+1) )
                        if val > 100: val = 100
                        data_dim.append(val)
                        found = 1
                if not found:
                    print("value NOT found i:%d j:%d" % (i,j))
                    data_dim.append(-1)
        data.append(data_dim)

    print("\nPARSED:\n")
    data = np.array(data)
    fig, axis = plt.subplots()
    if lower_bound:
        heatmap = axis.imshow(data, cmap=plt.cm.Blues, interpolation=None, vmin=lower_bound, vmax=100) # heatmap values
    else:
        heatmap = axis.imshow(data, cmap=plt.cm.Blues, interpolation=None) # heatmap values
    
    # Uncomment this to show all core numbers!!
    seq = np.arange(data.shape[1])+0
    axis.set_yticks(seq, minor=False)
    axis.set_xticks(seq, minor=False)

    #axis.invert_yaxis()
    #axis.invert_xaxis()
    #axis.set_yticklabels(row_labels, minor=False)
    #axis.set_xticklabels(column_labels, minor=False)
    plt.xticks(fontsize=font_size-4,rotation = 15)
    plt.yticks(fontsize=font_size-4)
    # plt.setp(axis.get_xticklabels(), rotation=300, ha="left",rotation_mode="anchor", fontsize=2)

    fig.set_size_inches(6, 6)
    plt.colorbar(heatmap)
    name = "%s utilization" % metric
    name = name + " (\% of time)"
    axis.set_title(name, fontsize=font_size)
    fig_name = "plots/heatmaps/heatmap2_" + dataset + "--" + str(mesh) + "-X-" + str(mesh)+ "-" + app + "-"+metric +"-"+binary +".pdf"
    print(fig_name)
    plt.savefig(fig_name, bbox_inches='tight')

def main(argv):
    lower_bound = 0
    meshes = [16]
    apps = ["sssp","bfs","wcc","pagerank","spmv"]
    datasets = ["wikipedia", "liveJournal", "Kron22"]
    metrics = ["Router","PU"]
    binaries = ["01", "ME"]
    usage = "heatmap.py -m <mesh> -a <app> -d <dataset> -t <metric> -b <binary> -f (lower_bound)"
    try:
        opts, args = getopt.getopt(argv,"hm:a:d:t:b:f:",["mesh=","app="])
    except getopt.GetoptError:
        print(usage)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print(usage)
            sys.exit()
        elif opt in ("-m", "--mesh_x"):
            meshes = [int(arg)]
        elif opt in ("-a", "--app"):
            apps = [arg]
        elif opt in ("-d", "--dataset"):
            datasets = [arg]
        elif opt in ("-b", "--binary"):
            binaries = [arg]
        elif opt in ("-f", "--lower_bound"):
            lower_bound = arg
        elif opt in ("-t", "--metric"):
            metrics = [arg]
            if arg not in metric_keys:
                print(arg + "is not a valid metric: ")
                sys.exit(2)
            
    for app in apps:
        for dataset in datasets:
            for mesh in meshes:
                for metric in metrics:
                    for binary in binaries:
                        print('Mesh is "', mesh)
                        print('App is "', app)
                        print('Dataset is "', dataset)
                        print('Metric is "', metric)
                        print('Binary is "', binary)
                        plot(app,dataset,mesh,metric,binary,lower_bound)

if __name__ == "__main__":
   main(sys.argv[1:])