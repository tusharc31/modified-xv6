Waitx and time:
For waitx I only needed to copy wait and add 2 lines.
Similarly in time command I only had to use exec and fork.

PS:
ps was implemented through a system call named "pscall" which looped through the processes
and printed their details.

***SCHEDULERS***:

**FCFS**: For this minimal changes had to be made. Just find runnable process
with lowest creation time.

**PBS**: Similar to FCFS but uses priority instead of creation time. Preemptiveness
is maintained through a global variable lowest_priority which stores the lowest priority
among RUNNABLE processes. On each clock tick the priority of running processes is comapre
with it so that only process with the lowest priorities are running.

**MLFQ**: MLFQ has been implemented according to the given conditions. Althought processes
may exploit this by sleeping just before their timeslice is over so that they still remain
in the lowest priority queue.

***GRAPH***:
Graph has been created using matplotlib in Jupyter notebook. The file "graph.png" contains
the graph. The file data.txt contains the data and "Untitled.ipynb" contains the code
with which the graph was made.

The data was collected by running ps command in trap.c whenever clock ticks were a multiple
of 50 and number of proccesses were greater than 4. The appropriate part has been commented
out and can be seen in the latest commit in trap.c file.

