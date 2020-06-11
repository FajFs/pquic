import matplotlib.pyplot as plt
import numpy as np

f = open("log_server.log")
NO_QUEUES = 8

time = list()
buffer = list()
dropps_memory = list()
dropps_codel = list()
cwin = list()
cwintime = list()

lines = f.readlines()

normalizeTime = 0
for l in lines:
    if"TIME:" in l:
        normalizeTime = float(l.strip().split(":")[1]) 
        break

no_dropped_memory = 0
drop_time_memory = list()
no_dropped_codel = 0
drop_time_codel = list()

queues = {}
for i in range(NO_QUEUES):
    queues[i] = list()

sjournTimeQueues = {}
for i in range(NO_QUEUES):
    sjournTimeQueues[i] = list()
sjournTime = {}
for i in range(NO_QUEUES):
    sjournTime[i] = list()

currTime = 0
for l in lines:
    if "TIME:" in l:
        currTime = (float(l.strip().strip(":").split()[1]) - normalizeTime) / 1000000
        time.append(currTime)

    if "BUFFER:" in l:
        buffer.append(float(l.strip().strip(":").split()[1]))

    if "DROPPED_MEMORY:" in l:
        if no_dropped_memory < float(l.strip().strip(":").split()[1]):
            dropps_memory.append(float(l.strip().strip(":").split()[1]))
            drop_time_memory.append(currTime) 
            no_dropped_memory = float(l.strip().strip(":").split()[1])
    
    if "DROPPED_CODEL:" in l:
        #if no_dropped_codel < float(l.strip().strip(":").split()[1]):
        tmp =l.strip().split()
        dropps_codel.append(float(tmp[1]))
        drop_time_codel.append((float(tmp[2])- normalizeTime)/1000000) 
        no_dropped_codel = float(tmp[1])
    
    if l.strip().startswith("Q:"):
        tmp = l.strip().split()
        queues[int(tmp[1])].append(float(tmp[3]))
    
    if "cwin" in l:
        cwin.append(float(l.strip().split()[1]))
        cwintime.append((float(l.strip().split()[2]) - normalizeTime) / 1000000) 
    
    if "SJOURN_TIME" in l:
        tmp = l.strip().split()
        sjournTimeQueues[int(tmp[3])].append(float(tmp[1])/ 1000)
        sjournTime[int(tmp[3])].append((float(tmp[2])- normalizeTime) / 1000000)


        
# print("DROPPED MEMORY= " , no_dropped_memory)
# print("DROPPED CODEL= " , no_dropped_codel)

#plt.plot(cwintime, cwin, linewidth=0.5)
#plt.plot(time, buffer, linewidth=0.1)

fig, axs = plt.subplots(1)
##SETUP PLOT FOR QUEUE MANAGEMENT##
# for i,q in enumerate(queues):
#     axs[0].plot(time, queues[q], linewidth=1, label="Flow " + str(i))
#     axs[0].legend(loc="upper right")
# axs[0].axhline(y=0, color = "black")
# axs[0].set(xlabel='Time (s)', ylabel='Queue Size in Bytes')
# axs[0].set_xlim(left=0)

# axs[0].plot(drop_time_memory, dropps_memory, "rx", linewidth=1)
# axs[0].plot(drop_time_codel, dropps_codel, "rx", linewidth=1, label="CoDel Drop")
# axs[0].legend(loc="upper right")
# axs[0].set_title("Queue Evolution Over Time")

##SETUP PLOT FOR SJOURN TIME##
# for i,s in enumerate(sjournTimeQueues):
#      axs[0].plot(sjournTime[s], sjournTimeQueues[s], linewidth=1, label="Flow " + str(i) )
#      axs[0].legend(loc="upper right")
# axs[0].axhline(y=5, label='Target Time',color="black")
# axs[0].legend(loc="upper right")
# axs[0].axhline(y=0, color = "black")
# axs[0].set(xlabel='Time (s)', ylabel='Sjourn Time (ms)')
# axs[0].set_xlim(left=0)
# axs[0].set_ylim(top=100, bottom=-5)

# axs[0].plot(drop_time_memory, dropps_memory, "rx", linewidth=1)
# axs[0].plot(drop_time_codel, dropps_codel, "rx", linewidth=1, label="CoDel Drop")
# axs[0].legend(loc="upper right")
# axs[0].set_title("Sjourn Time Evolution")


for i,s in enumerate(sjournTimeQueues):
     plt.plot(sjournTime[s], sjournTimeQueues[s], linewidth=1, label="CoDel Queue " + str(i) )
     plt.legend(loc="upper right", fontsize=18)
plt.axhline(y=5, label='Target Time (5ms)',color="black")
plt.legend(loc="upper right", fontsize=18)
plt.axhline(y=0, color = "black")
plt.xlabel('Time (s)', fontsize=18) 
plt.ylabel('Sjourn Time (ms)', fontsize=18)
#plt.set_xlim(left=0)
#plt.set_ylim(top=100, bottom=-5)

plt.plot(drop_time_memory, dropps_memory, "rx", linewidth=1)
plt.plot(drop_time_codel, dropps_codel, "rx", linewidth=1, label="CoDel Drop")
plt.legend(loc="upper right", fontsize=18)
# plt.title("Frame Sjourn Time Evolution over Per Queue O")



# plt.plot(time, reserved_frames, linewidth=5, label="QUIC reserved_frames")
# plt.legend(loc="upper left", prop={"size":18})

# plt.plot(time, retry_frames, linewidth=5, label="QUIC retry_frames")
# plt.legend(loc="upper left", prop={"size":18})

# plt.plot(time, block_queue_cc, linewidth=5, label="Datagram block_queue_cc")
# plt.legend(loc="upper left", prop={"size":18})

# plt.plot(time, block_queue_non_cc, linewidth=5, label="Datagram block_queue_non_cc")
# plt.legend(loc="upper left", prop={"size":18})

# plt.axhline(y=0, color = "black")
# plt.xlabel('Time (s)',fontsize=18)
# plt.ylabel('Number of frames queued', fontsize=18)

#plt.legend(loc="upper right")
#plt.suptitle("Queue Evolution Over Time")
plt.rcParams.update({'font.size': 22})
plt.xticks(fontsize=18)
plt.yticks(fontsize=18)
plt.show()




plt.show()
f.close()