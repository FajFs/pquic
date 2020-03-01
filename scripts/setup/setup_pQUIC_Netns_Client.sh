#!/bin/bash
#Run script after reboot

# SETUP
#           /IF1 <-->  IF1\        
# 	CLIENT -               - MITM - IF3  <-->  IF1 - PROXY
#           \IF2 <-->  IF2/

### All ip adresses uses a /24 as subnetmask ###

#Name of Namespace created
NS=pquicns

#Used to run in namespace
EXEC_NETNS="ip netns exec pquicns"

#Name and ip of physical interfaces on client
IF_1_NAME_CLIENT=enp1s0
IF_1_IP_CLIENT=10.0.1.1
IF_1_SUBNET_CLIENT=10.0.1.0/24

IF_2_NAME_CLIENT=enp3s0
IF_2_IP_CLIENT=10.0.2.1
IF_2_SUBNET_CLIENT=10.0.2.0/24

IF_1_IP_MITM=10.0.1.2
IF_2_IP_MITM=10.0.2.2

#Name and ip of Tun interface
TUN_NAME=tun0
TUN_IP_CLIENT=10.0.5.1
TUN_IP_PROXY=10.0.5.2

#Add the namespace (ns)
ip netns del $NS
ip netns add $NS

#Add the physical interfaces to the ns
ip link set $IF_1_NAME_CLIENT netns $NS
ip link set $IF_2_NAME_CLIENT netns $NS

#Set the ip address of all physical interfaces
$EXEC_NETNS ip addr add ${IF_1_IP_CLIENT}/24 dev $IF_1_NAME_CLIENT
$EXEC_NETNS ip addr add ${IF_2_IP_CLIENT}/24 dev $IF_2_NAME_CLIENT

#Enable intefaces 
$EXEC_NETNS ip link set $IF_1_NAME_CLIENT up
$EXEC_NETNS ip link set $IF_2_NAME_CLIENT up
$EXEC_NETNS ip link set lo up

#setup ${TUN_NAME} interface
$EXEC_NETNS modprobe tun
$EXEC_NETNS ip tuntap add mode tun dev ${TUN_NAME}
$EXEC_NETNS ip addr add ${TUN_IP_CLIENT}/24 dev ${TUN_NAME}
$EXEC_NETNS ip link set dev ${TUN_NAME} mtu 1400
$EXEC_NETNS ip link set dev ${TUN_NAME} up

#add route to vpn
$EXEC_NETNS ip route add ${TUN_IP_PROXY} via ${TUN_IP_CLIENT} dev ${TUN_NAME}

#set up utilization of multiple using routing rules
$EXEC_NETNS ip rule add from ${IF_1_IP_CLIENT} table 1
$EXEC_NETNS ip route add ${IF_1_SUBNET_CLIENT} dev $IF_1_NAME_CLIENT scope link table 1
$EXEC_NETNS ip route add default via ${IF_1_IP_MITM} dev $IF_1_NAME_CLIENT table 1

$EXEC_NETNS ip rule add from ${IF_2_IP_CLIENT} table 2
$EXEC_NETNS ip route add ${IF_2_SUBNET_CLIENT} dev $IF_2_NAME_CLIENT scope link table 2
$EXEC_NETNS ip route add default via ${IF_2_IP_MITM} dev $IF_2_NAME_CLIENT table 2

#Following lines are critical for multipathing
$EXEC_NETNS ip route add default via ${IF_1_IP_MITM} dev $IF_1_NAME_CLIENT metric 100
$EXEC_NETNS ip route add default via ${IF_2_IP_MITM} dev $IF_2_NAME_CLIENT metric 101