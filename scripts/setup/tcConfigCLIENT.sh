#!/bin/bash

# SETUP
#           /IF1 <-->  IF1\
#   CLIENT -               - MITM - IF3  <-->  IF1 - PROXY
#           \IF2 <-->  IF2/

#Parameters configures the links between the CLIENT node and the MITM node
IF1_EGRESS=enp1s0
IF1_INGRESS=ifb0
IF2_EGRESS=enp3s0
IF2_INGRESS=ifb1

BANDWIDTH_LINK_1=$1
BANDWIDTH_LINK_2=$2

LOSS_LINK_1="loss ${3}%"
LOSS_LINK_2="loss ${4}%"
#Must check for 0% loss since this causes issues with netem
if [ "$3" == "0" ];	then
    LOSS_LINK_1=""
fi
if [ "$4" == "0" ];	then
    LOSS_LINK_2=""
fi

DELAY_LINK_1=$5
DELAY_LINK_2=$6

# Set queue size according to pQUIC formula
QUEUE_SIZE_LINK_1=$(python3 -c "print(int(1.5 * (((${BANDWIDTH_LINK_1} * 1000000) / 8) / 1500) * (2 * 70 / 1000.0)))")
QUEUE_SIZE_LINK_2=$(python3 -c "print(int(1.5 * (((${BANDWIDTH_LINK_2} * 1000000) / 8) / 1500) * (2 * 70 / 1000.0)))")

if [ "$1" == "h" ] || [ "$1" == "H" ];	then
    echo The script will form the traffic, egress/ingress, of two interfaces on the CLIENT node
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

#On the client all commands must be executed in the pquicns
NS="ip netns exec pquicns "

modprobe ifb
#probing can't be done directly into pquicns therefore the ifb interfaces are moved to the pquicns
ip link set $IF1_INGRESS netns pquicns
ip link set $IF2_INGRESS netns pquicns

#enable the ifb interfaces
$NS ip link set ifb0 up
$NS ip link set ifb1 up

# clear TC configurations
$NS tc qdisc del dev $IF1_EGRESS root &> /dev/null
$NS tc qdisc del dev $IF1_EGRESS ingress &> /dev/null
$NS tc qdisc del dev $IF2_EGRESS root &> /dev/null
$NS tc qdisc del dev $IF2_EGRESS ingress &> /dev/null
$NS tc qdisc del dev $IF1_INGRESS root &> /dev/null
$NS tc qdisc del dev $IF2_INGRESS root &> /dev/null

# Configure egress traffic
$NS tc qdisc add dev $IF1_EGRESS root handle 1: htb default 1
$NS tc class add dev $IF1_EGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_1}mbit
$NS tc qdisc add dev $IF1_EGRESS parent 1:1 handle 10: netem limit ${QUEUE_SIZE_LINK_1} delay ${DELAY_LINK_1}ms  ${LOSS_LINK_1}

$NS tc qdisc add dev $IF2_EGRESS root handle 1: htb default 1
$NS tc class add dev $IF2_EGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_2}mbit
$NS tc qdisc add dev $IF2_EGRESS parent 1:1 handle 10: netem limit ${QUEUE_SIZE_LINK_2} delay ${DELAY_LINK_2}ms  ${LOSS_LINK_2}

# forward icomming traffic to ifb for limiting
$NS tc qdisc add dev $IF1_EGRESS ingress
$NS tc filter add dev $IF1_EGRESS parent ffff: protocol all u32 match u32 0 0 action mirred egress redirect dev $IF1_INGRESS
$NS tc qdisc add dev $IF2_EGRESS ingress
$NS tc filter add dev $IF2_EGRESS parent ffff: protocol all u32 match u32 0 0 action mirred egress redirect dev $IF2_INGRESS

# limit all ingress
$NS tc qdisc add dev $IF1_INGRESS root handle 1: htb default 1
$NS tc class add dev $IF1_INGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_1}mbit
$NS tc qdisc add dev $IF1_INGRESS parent 1:1 handle 10: netem limit ${QUEUE_SIZE_LINK_1} delay ${DELAY_LINK_1}ms  ${LOSS_LINK_1}

$NS tc qdisc add dev $IF2_INGRESS root handle 1: htb default 1
$NS tc class add dev $IF2_INGRESS parent 1:1 classid 1:1 htb rate ${BANDWIDTH_LINK_2}mbit
$NS tc qdisc add dev $IF2_INGRESS parent 1:1 handle 10: netem limit ${QUEUE_SIZE_LINK_2} delay ${DELAY_LINK_2}ms  ${LOSS_LINK_2}

#QUEUE_SIZE_LINK_1=1000
#QUEUE_SIZE_LINK_2=1000
## Egress queue size
#$NS ip link set txqueuelen $QUEUE_SIZE_LINK_1 dev $IF1_EGRESS
#$NS ip link set txqueuelen $QUEUE_SIZE_LINK_2 dev $IF2_EGRESS
## Ingress queue size
#$NS ip link set txqueuelen $QUEUE_SIZE_LINK_1 dev $IF1_INGRESS
#$NS ip link set txqueuelen $QUEUE_SIZE_LINK_2 dev $IF2_INGRESS
