#!/bin/bash
killall picoquicvpn
killall tcpdump
../scripts/setup/tcConfigCLIENT.sh $1 $2 $3 $4 $5 $6
#ip netns exec pquicns ./picoquicvpn -P plugins/datagram/datagram.plugin -P plugins/Simple/simple.plugin 10.0.3.1 4443 &> out.log &
#ip netns exec pquicns ./picoquicvpn -P plugins/datagram/datagram.plugin -P plugins/multipath/multipath_priority.plugin 10.0.3.1 4443 &> out.log &
ip netns exec pquicns ./picoquicvpn -P plugins/datagram/datagram.plugin -P plugins/multipath/multipath_rr.plugin 10.0.3.1 4443 &> out.log &

sleep 5
ip netns exec pquicns tcpdump -i enp1s0 -w interface1.pcap &
ip netns exec pquicns tcpdump -i enp3s0 -w interface2.pcap &

#udp
ip netns exec pquicns iperf -c 10.0.5.2 -u -b $7m -t 20 -l 1350
#tcp
#ip netns exec pquicns iperf -c 10.0.5.2 -t 20 -l 1350

killall tcpdump
sudo -u student ../scripts/process/turn_pcaps_into_time_graph.py interface1.pcap interface2.pcap

capinfos -c interface1.pcap | awk /Numb/ && capinfos -c interface2.pcap | awk /Numb/
