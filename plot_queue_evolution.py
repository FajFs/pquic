import matplotlib.pyplot as plt
import numpy as np

f = open("log_server.log")

#    printf("time: %lu\n", picoquic_current_time());
#    printf("reserved_frames: %u\n",cnx->reserved_frames->size);
#    printf("retry_frames: %u\n",cnx->retry_frames->size);
#    printf("block_queue_cc: %u\n",p->block_queue_cc->size);
#    printf("block_queue_non_cc: %u\n",p->block_queue_non_cc->size);

time = list()
reserved_frames = list()
retry_frames = list()
block_queue_cc = list()
block_queue_non_cc = list()

lines = f.readlines()

normalizeTime = 0
for l in lines:
    if"time:" in l:
        normalizeTime = float(l.strip().split(":")[1]) 
        break

currTime = 0
for l in lines:
    if "time:" in l:
        currTime = ((float(l.strip().split()[1]) - normalizeTime) / 1000000) 
        time.append(currTime - 5000000)

    if "reserved_frames:" in l:
        reserved_frames.append(float(l.split()[1]))

    if "retry_frames:" in l:
        retry_frames.append(float(l.split()[1]))    
    
    if "block_queue_cc:" in l:
        block_queue_cc.append(float(l.strip().split()[1]))
   
    if "block_queue_non_cc:" in l:
        block_queue_non_cc.append(float(l.split()[1]))
    


##SETUP PLOT FOR QUEUE MANAGEMENT##
plt.plot(time, reserved_frames, linewidth=5, label="QUIC reserved_frames")
plt.legend(loc="upper left", prop={"size":18})

plt.plot(time, retry_frames, linewidth=5, label="QUIC retry_frames")
plt.legend(loc="upper left", prop={"size":18})

plt.plot(time, block_queue_cc, linewidth=5, label="Datagram block_queue_cc")
plt.legend(loc="upper left", prop={"size":18})

plt.plot(time, block_queue_non_cc, linewidth=5, label="Datagram block_queue_non_cc")
plt.legend(loc="upper left", prop={"size":18})

plt.axhline(y=0, color = "black")
plt.xlabel('Time (s)',fontsize=18)
plt.ylabel('Number of frames queued', fontsize=18)

#plt.legend(loc="upper right")
#plt.suptitle("Queue Evolution Over Time")
plt.rcParams.update({'font.size': 22})
plt.xticks(fontsize=18)
plt.yticks(fontsize=18)
plt.show()
f.close()