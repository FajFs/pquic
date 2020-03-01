#!/bin/bash
#Run script after reboot

# SETUP
#               /IF1 <-->  IF1\        
# 	CLIENT -               - MITM - IF3  <-->  IF1 - PROXY
#               \IF2 <-->  IF2/

### All ip adresses uses a /24 as subnetmask ###
STATE=up
RULE=add

if [ "$1" = "-d" ]
then
        STATE=down
        RULE=del
fi

IF_1_IP_MITM=10.0.1.2/24
IF_1_NAME_MITM=enp1s0f0

IF_2_IP_MITM=10.0.2.2/24
IF_2_NAME_MITM=enp3s0f0


IF_3_IP_MITM=10.0.3.2/24
IF_3_NAME_MITM=enp1s0f1

#Set the ip address of all interfaces
ip addr $RULE $IF_1_IP_MITM dev $IF_1_NAME_MITM
ip addr $RULE $IF_2_IP_MITM dev $IF_2_NAME_MITM

ip addr $RULE $IF_3_IP_MITM dev $IF_3_NAME_MITM
#ip addr $RULE $MITM_PROXY_ADDR_2 dev $MITM_PROXY_INT_2

#Enable interfaces
ip link set $IF_1_NAME_MITM $STATE
ip link set $IF_2_NAME_MITM $STATE
ip link set $IF_3_NAME_MITM $STATE
#ip link set $MITM_PROXY_INT_2 $STATE

#Enable FORWARDING
sysctl -w net.ipv4.ip_forward=1
