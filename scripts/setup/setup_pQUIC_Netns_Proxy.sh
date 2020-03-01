#!/bin/bash
#Run script after reboot

# SETUP
#           /IF1 <-->  IF1\        
#   CLIENT -               - MITM - IF3  <-->  IF1 - PROXY
#           \IF2 <-->  IF2/

#Name of Namespace created
NS=pquicns

#Used to run in namespace
EXEC_NETNS="ip netns exec pquicns"

#Name and ip of the physical interface on the proxy
IF_1_NAME_PROXY=enp1s0
IF_1_IP_PROXY=10.0.3.1/24

#IP of the physical interfaces on the MITM
IF_3_IP_MITM=10.0.3.2

#IP of the pysical interfaces on the client
IF_1_IP_CLIENT=10.0.1.1
IF_2_IP_CLIENT=10.0.2.1

#Name and ip of Tun Interface
TUN_NAME=tun1
TUN_IP_PROXY=10.0.5.2/24

#Add the namespace (ns)
ip netns del $NS
ip netns add $NS
ip link set $IF_1_NAME_PROXY netns $NS

#Set the ip address of all interfaces
$EXEC_NETNS ip addr add $IF_1_IP_PROXY dev $IF_1_NAME_PROXY

#Enable intefaces
$EXEC_NETNS ip link set $IF_1_NAME_PROXY up

#setup tun1 interface
modprobe tun
$EXEC_NETNS ip tuntap add mode tun dev $TUN_NAME

$EXEC_NETNS ip addr add $TUN_IP_PROXY dev $TUN_NAME
$EXEC_NETNS ip link set dev $TUN_NAME mtu 1400
$EXEC_NETNS ip link set dev $TUN_NAME up

#Set default gateway
$EXEC_NETNS ip route add $IF_1_IP_CLIENT via $IF_3_IP_MITM dev $IF_1_NAME_PROXY
$EXEC_NETNS ip route add $IF_2_IP_CLIENT via $IF_3_IP_MITM dev $IF_1_NAME_PROXY

#enable FORWARDING
sysctl -w net.ipv4.ip_forward=1