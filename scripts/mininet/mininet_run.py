#!/bin/bash
python2 ../scripts/mininet/topoCMP_experiment.py
sudo -u student ../scripts/process/turn_pcaps_into_time_graph.py interface1.pcap interface2.pcap
