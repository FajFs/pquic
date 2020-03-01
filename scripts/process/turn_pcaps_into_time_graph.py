#!/usr/bin/python3
from matplotlib import pyplot as plt
from sys import argv
import subprocess
import os

pltTitle = "Throughput per unit time for two interface"
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

def saturate_lists(xList, yList, csv):
    unit = 1000000
    lines = csv.split("\n")
    for line in lines:
        values = line.split(",")
        try:
            xList.append(float(values[0]))
            yList.append(8*float(values[1])/unit)
        except Exception as e:
            print("Could not convert " + values[0] + " to float")

#------ Main ------#
pcap1=argv[1]
pcap2=argv[2]

xList1 = []
yList1 = []

xList2 = []
yList2 = []

#Get csv format
csv1 = subprocess.getoutput("tshark -nr " + pcap1 + " -q -z io,stat,1,BYTES | grep -P \"\d+\s+<>\s+[\dDur]+\s*\|\s+\d+\" | awk -F \'[ |]+\' \'{print $2\",\"($5)}\'")
csv2 = subprocess.getoutput("tshark -nr " + pcap2 + " -q -z io,stat,1,BYTES | grep -P \"\d+\s+<>\s+[\dDur]+\s*\|\s+\d+\" | awk -F \'[ |]+\' \'{print $2\",\"($5)}\'")
saturate_lists(xList1, yList1, csv1)
saturate_lists(xList2, yList2, csv2)

drawDiagram(xList1, yList1, xList2, yList2)

