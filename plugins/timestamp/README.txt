Simple One way delay difference calculation for multipath system to find the difference between the slowest and fastest path.

timestamp.plugin is attached to datagram.plugin - every time a datagram is sent, a timestamp is attached

the only modification to the datagram plugin is the prootop call to reserve a timestamp frame, look send_datagram.c
the value is stored in tm->max1wdiff
