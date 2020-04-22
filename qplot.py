import matplotlib.pyplot as plt
import numpy as np

f = open("log_server.log")

time = list()
buffer = list()
dropps = list()
cwin = list()
cwintime = list()

lines = f.readlines()

normalizeTime = 0
for l in lines:
    if"TIME:" in l:
        normalizeTime = float(l.strip().split(":")[1]) 
        break

no_dropped = 0
drop_time = list()

queues = {}
for i in range(8):
    queues[i] = list()

currTime = 0
for l in lines:
    if "TIME:" in l:
        currTime = (float(l.strip().strip(":").split()[1]) - normalizeTime) / 1000000
        time.append(currTime)


    if "BUFFER:" in l:
        buffer.append(float(l.strip().strip(":").split()[1]))

    if "DROPPED:" in l:
        if no_dropped < float(l.strip().strip(":").split()[1]):
            dropps.append(float(l.strip().strip(":").split()[1]))
            drop_time.append(currTime) 
            no_dropped = float(l.strip().strip(":").split()[1])
    if l.strip().startswith("Q:"):
        tmp = l.strip().split()
        queues[int(tmp[1])].append(float(tmp[3]))
    if "cwin" in l:
        cwin.append(float(l.strip().split()[1]))
        cwintime.append((float(l.strip().split()[2]) - normalizeTime) / 1000000) 

        
#print(queues[1])

#remove dead time


print("DROPPED = " , no_dropped)

plt.plot(cwintime, cwin, linewidth=0.5)

#plt.plot(time, buffer, linewidth=0.1)

plt.plot(drop_time, dropps, "rx", linewidth=0.1)
for q in queues:
    plt.plot(time, queues[q], linewidth=0.1)
plt.xlabel("Time (s)")
plt.ylabel("Queue Size in Bytes")
plt.show()

f.close()