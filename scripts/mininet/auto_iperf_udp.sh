r1 tcpdump -i r1-eth0 -w interface1.pcap &
r1 tcpdump -i r1-eth1 -w interface2.pcap &
vpn iperf -s -u &
cl iperf -c 10.0.5.2 -u -b 450m -t 20 >> out.log
cl capinfos -i interface1.pcap >> out.log
cl capinfos -i interface2.pcap >> out.log

