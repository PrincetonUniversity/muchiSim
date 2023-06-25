
## Terminology

### Tile Grid

Application program is executed on a grid: a logical 2D array of tiles. The physical organization of this logical 2D array depends on other structural concepts like nodes, chip packages, and chiplets (explained later). 
 
### Tile

Each tile contains a Processing Unit (PU), a scratchpad SRAM memory (SPM), a Router and a task scheduling unit (TSU). 
 
### Router

It contains five ports: North, South, East, West, and PU ports. The routers at the edges of a chiplet contain additional ports for express links (one hop between chiplets). 

### Task Scheduling Unit (TSU) 

The TSU moves messages from the router to the input queues (IQq) and from the output queues (OQs).  The TSU also contains the policy for which task the PU should execute next, based on the ones readily available at the IQs (one IQ per task type). 
 
### Scratchpad Memory (SPM)

Its size is specified to the simulator in Kibibytes (KiB). There is no model for bank conflicts. The IQs and OQs are mapped into the SPM as circular FIFOs. 
 
### Modeling the Processing Unit (PU)

Tasks are executed on a Processing Unit (PU). The PU is modeled in a simplistic manner by specifying the latency of computing the instructions within the task code. This is tedious but flexible since it is a quick way to model the latency of instructions within a task.

### Simultaneous Multi-threading (SMT)

A PU can have several logical threads that allows the PU to overlap the time that is waiting for data to switch to a different thread. Computation by different threads cannot be done simultaneously.

### Chiplet Die

muchiSim allows simulation of a hierarchical design, where a chiplet (also called a die) is the smallest unit that is fabricated. We only care about differentiating chiplets from chip packages if we care about faithfully modeling the power consumption and bandwidth limitations between them. Otherwise, we can model a chip package as a single chiplet.
There is a parameter to set the number of links between chiplets inside a package, and their width. 
Each chiplet contains a memory device with a number of channels that is configurable via a parameter. The width of the channel is matched with the width of a cache line, e.g., 32B. 

In addition to the Tile’s SPM, the simulator allows a per-chiplet level scratchpad, with a latency equal to the average distance between a tile to the edge of the chiplet and back, plus N cycles (a parameter) for scratchpad read. However, we assume that this scratchpad has enough banks and memory ports to not cause end-point contention for the tiles trying to read from it.
 
### Chip Package

A chip package contains a grid of chiplets (it can be as low as 1x1). 
There is a parameter to set the number of links between chiplets inside a chip package, and their width. 
 
### Node

A node contains a grid of chip packages (it can be as low as 1x1). There is a parameter to set the number of links between chip packages inside a node, and their width. Currently, there are only two network topology options to connect all tiles across chiplets and chips: 2D-mesh and 2D-torus.
However, we also support Ruche Channels. These are express channels that connect non-contiguous routers in a 2D network. For example, the regular 2D NoC connects every tile, they have a hop distance of 1 tile, while for ruche channels the hop distance might be several tiles. 
 
### Cluster

A cluster contains a grid of nodes (it can be as low as 1x1). 
Currently in the Simulator, nodes are connected on a 2D mesh topology, but this could be extended to model other low-diameter networks.

### Simulating a program

The application code to simulate is directly written inside the simulator code. A new application is added by including a new header file and including that with a preprocessor macro based on the command line arguments. This header file contains the different tasks of a program. These tasks are invoked upon receiving a task invocation message into its corresponding input queue (IQ). A main task is also invoked only once at the beginning. The main task could be empty if the entire program only comprises tasks invoked by other tasks. On the contrary, a program can be entirely included in the main task (not invoking other tasks). Inside this program, we need at least to specify each instruction's penalty (latency).  
For memory operations, we use a special function that returns the data of a memory address but also returns the latency of the memory operation based on one of the five memory operation modes. 
Stores also use a special function that returns the latency of this operation, adds memory contention, and keeps track of energy consumption.

### Modeling contention for the memory channel of the memory controller

Since many tiles share a memory channel, the contention is modeled by considering that the memory channel can only take one request per cycle. We keep a count of the transactions of each memory channel. For example, if a request is done at cycle X, but the memory channel has received Y transactions (where Y > X), then the delay if this request is Y-X cycles + the round-trip to the memory channel. 