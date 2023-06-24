import networkx as nx
import numpy as np

Data = open('datasets/Kron16/Kron16.tsv',"r")
G = nx.parse_edgelist(Data, comments='c', create_using=nx.DiGraph, nodetype=int, data=(('weight', int),))
num_nodes=max(G.nodes())+1

ret = np.full((num_nodes,1),-1, dtype=np.int32)

H = nx.weakly_connected_components(G)

cc_dict=dict();

i=-1
for cc in H:
    i+=1
    for node in sorted(cc):
        cc_dict[i]=node
        break

cc_dict

H = nx.weakly_connected_components(G)

i=-1
for cc in H:
    i+=1
    for node in cc:
        ret[node]=cc_dict[i]
        if (node==48617):
            print("Component for 48617 is " + str(cc_dict[i]))

for j in range(num_nodes):
    if (ret[j]==-1):
        ret[j]=j

np.savetxt("WCC_python16.tsv", ret,  fmt='%d')

