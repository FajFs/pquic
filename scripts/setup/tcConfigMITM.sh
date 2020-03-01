#!/bin/bash

# SETUP
#           /IF1 <-->  IF1\
#   CLIENT -               - MITM - IF3  <-->  IF1 - PROXY
#           \IF2 <-->  IF2/

#Parameters configures the links between the CLIENT node and the MITM node
IF1_EGRESS=enp1s0f0
IF1_INGRESS=ifb0
IF2_EGRESS=enp3s0f0
IF2_INGRESS=ifb1

BANDWIDTH_LINK_1=$1
BANDWIDTH_LINK_2=$2

LOSS_LINK_1="loss ${3}%"
LOSS_LINK_2="loss ${4}%"4
#Must check for 0% loss since this causes issues with netem
if [ "$3" == "0" ];	then
    LOSS_LINK_1=""
fi
if [ "$4" == "0" ];	then
    LOSS_LINK_2=""
fi

DELAY_LINK_1=$5
DELAY_LINK_2=$6

if [ "$1" == "h" ] || [ "$1" == "H" ];	then
    echo The script will form the traffic, egress/ingress, of two interfaces on the MITM node
    echo SETUP
    echo "           IF1 <-->  IF1"
    echo " CLIENT -                 - MITM - IF3  <-->  IF1 - PROXY "
    echo "           IF2 <-->  IF2"
    echo params: BW_IF1 mbps, BW_IF2 mbps, LOSS_IF1 %, LOSS_IF2 %, DELAY_IF1 ms, DELAY_IF2 ms
    exit
fi

if [ "$#" -ne 6 ]; then
    echo ERROR: You must supply 6 params
    echo h/H for help
    echo params: BW_IF1 mbps, BW_IF2 mbps, LOSS_IF1 %, LOSS_IF2 %, DELAY_IF1 ms, DELAY_IF2 ms
    exit
fi

modprobe ifb
ip link set ifb0 up
ip link set ifb1 up

# clear TC configurations
tc qdisc del dev $IF1_EGRESS root &> /dev/null
tc qdisc del dev $IF1_EGRESS ingress &> /dev/null
tc qdisc del dev $IF2_EGRESS root &> /dev/null
tc qdisc del dev $IF2_EGRESS ingress &> /dev/null
tc qdisc del dev $IF1_INGRESS root &> /dev/null
tc qdisc del dev $IF2_INGRESS root &> /dev/null

# Configure egress traffic
tc qdisc add dev $IF1_EGRESS root handle 1: htb default 1
tc class add dev $IF1_EGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_1}mbit
tc qdisc add dev $IF1_EGRESS parent 1:1 handle 10: netem delay ${DELAY_LINK_1}ms  ${LOSS_LINK_1}

tc qdisc add dev $IF2_EGRESS root handle 1: htb default 1
tc class add dev $IF2_EGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_2}mbit
tc qdisc add dev $IF2_EGRESS parent 1:1 handle 10: netem delay ${DELAY_LINK_2}ms  ${LOSS_LINK_2}

# forward icomming traffic to ifb for limiting
tc qdisc add dev $IF1_EGRESS ingress
tc filter add dev $IF1_EGRESS parent ffff: protocol all u32 match u32 0 0 action mirred egress redirect dev $IF1_INGRESS
tc qdisc add dev $IF2_EGRESS ingress
tc filter add dev $IF2_EGRESS parent ffff: protocol all u32 match u32 0 0 action mirred egress redirect dev $IF2_INGRESS

# limit all ingress
tc qdisc add dev $IF1_INGRESS root handle 1: htb default 1
tc class add dev $IF1_INGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_1}mbit
tc qdisc add dev $IF1_INGRESS parent 1:1 handle 10: netem delay ${DELAY_LINK_1}ms  ${LOSS_LINK_1}

tc qdisc add dev $IF2_INGRESS root handle 1: htb default 1
tc class add dev $IF2_INGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_2}mbit
tc qdisc add dev $IF2_INGRESS parent 1:1 handle 10: netem delay ${DELAY_LINK_2}ms  ${LOSS_LINK_2}
