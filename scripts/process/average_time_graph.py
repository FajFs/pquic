#/usr/bin/python3
from matplotlib import pyplot as plt
from sys import argv
import subprocess
import os

pltTitle = "Througput per unit time for two interface"
pltXLabel = "Time (second)"
pltYLabel = "Throughput (Mb)"
outputFilename = "graph"

def drawDiagram(xList1, yList1, xList2, yList2):

    plt.plot(xList1, yList1, label="Interface 1")
    plt.plot(xList2, yList2, label="Interface 2")

    plt.title(pltTitle)
    plt.xlabel(pltXLabel)
    plt.ylabel(pltYLabel)
    plt.legend()
    plt.tight_layout()
    plt.xticks(xList1)

    plt.savefig(outputFilename + ".png", dpi=300)

def put_values_together(csv):
    unit = 1000000
    lines = csv.split("\n")
    val_dict = {}
    for line in lines:
        if ',' not in line:
            continue
        if not line[0].isdigit():
            continue
        values = line.split(",")
        val_dict[int(values[0])] = 8*float(values[1])/unit
    return val_dict

def get_throughput_per_second(pcap):
    csv = subprocess.getoutput("tshark -nr " + pcap + " -q -z io,stat,1,BYTES | grep -P \"\d+\s+<>\s+[\dDur]+\s*\|\s+\d+\" | awk -F \'[ |]+\' \'{print $2\",\"($5)}\'")
    return put_values_together(csv)

def run():
    print(subprocess.getoutput("sudo ./mininet_run.py"))

#------ Main ------#
pcap1 = argv[1]
pcap2 = argv[2]

REPETITION = 5
SECONDS = 21

sum1 = {}
sum2 = {}

for _ in range(REPETITION):
    run()
    hashmap1 = get_throughput_per_second(pcap1)
    hashmap2 = get_throughput_per_second(pcap2)
    for x in hashmap1.keys():
        if x not in sum1:
            sum1[x] = 0
        sum1[x] += hashmap1[x]

    for x in hashmap2.keys():
        if x not in sum2:
            sum2[x] = 0
        sum2[x] += hashmap2[x]

for val in sum1.values():
    val /= REPETITION

for val in sum2.values():
    val /= REPETITION

#drawDiagram(xList1, yList1, xList2, yList2)
drawDiagram(list(sum1.keys()), list(sum1.values()), list(sum2.keys()), list(sum2.values()))
