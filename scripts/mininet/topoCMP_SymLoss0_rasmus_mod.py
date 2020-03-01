from time import sleep
import random
import os

from mininet.cli import CLI
from mininet.node import Node, OVSBridge
from mininet.link import TCLink
from mininet.log import setLogLevel
from mininet.net import Mininet
from mininet.topo import Topo

from mininet.clean import cleanup as net_cleanup
#mininet patch
from mininet.log import error
from mininet.link import TCIntf




@staticmethod
def delayCmds(parent, delay=None, jitter=None,
              loss=None, max_queue_size=None ):
    "Internal method: return tc commands for delay and loss"
    cmds = []
    if delay and delay < 0:
        error( 'Negative delay', delay, '\n' )
    elif jitter and jitter < 0:
        error( 'Negative jitter', jitter, '\n' )
    elif loss and ( loss < 0 or loss > 100 ):
        error( 'Bad loss percentage', loss, '%%\n' )
    else:
        # Delay/jitter/loss/max queue size
        netemargs = '%s%s%s%s' % (
            'delay %s ' % delay if delay is not None else '',
            '%s ' % jitter if jitter is not None else '',
            'loss %0.4f ' % loss if loss is not None and loss > 0 else '',  # The fix
            'limit %d' % max_queue_size if max_queue_size is not None
            else '' )
        if netemargs:
            cmds = [ '%s qdisc add dev %s ' + parent +
                     ' handle 10: netem ' +
                     netemargs ]
            parent = ' parent 10:1 '
    return cmds, parent
TCIntf.delayCmds = delayCmds
# end mininet patch


class LinuxRouter(Node):
    "A Node with IP forwarding enabled."
    def config(self, **params):
        super(LinuxRouter, self).config(**params)
        # Enable forwarding on the router
        self.cmd('sysctl net.ipv4.ip_forward=1')

    def terminate(self):
        self.cmd('sysctl net.ipv4.ip_forward=0')
        super(LinuxRouter, self).terminate()


class KiteTopo(Topo):
    def build(self, **opts):
        wifi_opt = {'delay': str(opts['delay_ms_a']) + "ms", 'loss': opts['loss_a'], 'max_queue_size': opts['queue_size_a'], 'bw': opts['bw_a']}
        lte_opt = {'delay': str(opts['delay_ms_b']) + "ms", 'loss': opts['loss_b'], 'max_queue_size': opts['queue_size_b'], 'bw': opts['bw_b']}

        self.r1 = self.addNode('r1', cls=LinuxRouter)
        self.cl = self.addHost('cl')
        self.vpn = self.addHost('vpn')

        self.addLink(self.cl, self.r1, intfName1='cl-eth0', intfName2='r1-eth0', **wifi_opt)
        self.addLink(self.cl, self.r1, intfName1='cl-eth1', intfName2='r1-eth1', **lte_opt)
        self.addLink(self.r1, self.vpn, intfName1='r1-eth2', intfName2='vpn-eth0')



def setup_ips(net):
    config = {
        'r1': {
            'r1-eth0': '10.0.1.2/24',
            'r1-eth1': '10.0.2.2/24',
            'r1-eth2': '10.0.3.2/24'
        },
        'cl': {
            'cl-eth0': '10.0.1.1/24',
            'cl-eth1': '10.0.2.1/24'
        },
        'vpn': {
            'vpn-eth0': '10.0.3.1/24'
        }
    }

    for h in config:
        for intf, ip in sorted(config[h].items(), key=lambda x: x[0] == 'default'):
            if intf != 'default':
                print net[h].cmd('ip addr flush dev {}'.format(intf))
                print net[h].cmd('ip addr add {} dev {}'.format(ip, intf))
            else:
                print net[h].cmd('ip route add default {}'.format(ip))


def setup_client_tun(nodes, id):
    tun_addr = '10.0.5.1/24'
    web_addr = '10.0.5.2'
    node = nodes[id]

    print node.cmd('modprobe tun')
    print node.cmd('ip tuntap add mode tun dev tun0')
    print node.cmd('ip addr add {} dev tun0'.format(tun_addr))
    print node.cmd('ip link set dev tun0 mtu 1400')
    print node.cmd('ip link set dev tun0 up')

    print node.cmd('ip route add {} via {} dev tun0'.format(web_addr, tun_addr[:-3]))

    print node.cmd('ip rule add from 10.0.1.1 table 1')
    print node.cmd('ip route add 10.0.1.0/24 dev cl-eth0 scope link table 1')
    print node.cmd('ip route add default via 10.0.1.2 dev cl-eth0 table 1')
    print node.cmd('ip rule add from 10.0.2.1 table 2')
    print node.cmd('ip route add 10.0.2.0/24 dev cl-eth1 scope link table 2')
    print node.cmd('ip route add default via 10.0.2.2 dev cl-eth1 table 2')

    # The two following lines are very important!
    print node.cmd('ip route add default via 10.0.1.2 dev cl-eth0 metric 102')
    print node.cmd('ip route add default via 10.0.2.2 dev cl-eth1 metric 101')


def setup_server_tun(nodes, id, server_addr):
    tun_addr = '10.0.5.2/24'
    node = nodes[id]

    print node.cmd('modprobe tun')
    print node.cmd('ip tuntap add mode tun dev tun1')
    print node.cmd('ip addr add {} dev tun1'.format(tun_addr))
    print node.cmd('ip link set dev tun1 mtu 1400')
    print node.cmd('ip link set dev tun1 up')
    print node.cmd('sysctl net.ipv4.ip_forward=1')

    print node.cmd('ip route add 10.0.1.1 via 10.0.3.2 dev vpn-eth0')
    print node.cmd('ip route add 10.0.2.1 via 10.0.3.2 dev vpn-eth0')


def ping_matrix(net):
    def ping_cmd(node, ip):
        return net[node].cmd('ping -i 0.25 -c 4 -w 2 -s 1472 {}'.format(ip))

    nodes = ('cl', 'web', 'vpn', 'r1', 'r2', 'r3')
    for n1 in nodes:
        for n2 in nodes:
            if n1 is n2:
                continue
            for ip in net[n2].cmd('ip addr | grep -o -P "10.\d+.\d+.\d+"').splitlines():
                print "%s -> %s @ %s" % (n1, n2, ip)
                print ping_cmd(n1, ip)


def setup_net(net, ip_tun=True, quic_tun=True, gdb=False, tcpdump=False, multipath=False, web_cmd='python3 -m http.server 80 &'):
    setup_ips(net)

    vpn_addr = '10.0.3.1'

    if ip_tun:
        setup_client_tun(net, 'cl')
        setup_server_tun(net, 'vpn', vpn_addr)


    if quic_tun and tcpdump:
        net['cl'].cmd('tcpdump -i cl-eth0 -w cl1.pcap &')
        net['cl'].cmd('tcpdump -i cl-eth1 -w cl2.pcap &')
        net['vpn'].cmd('tcpdump -i tun1 -w tun1.pcap &')
        net['r1'].cmd('tcpdump -i r1-eth0 -w r10.pcap &')
        net['r1'].cmd('tcpdump -i r1-eth1 -w r11.pcap &')
        net['r1'].cmd('tcpdump -i r1-eth2 -w r12.pcap &')
        net['r1'].cmd('tcpdump -i r1-eth3 -w r13.pcap &')
        net['vpn'].cmd('tcpdump -i vpn-eth0 -w vpn.pcap &')
        sleep(1)

    plugins = "-P plugins/datagram/datagram.plugin"
    if multipath:
        plugins += " -P plugins/multipath/multipath_rr.plugin"

    if quic_tun:
        if gdb:
            net['vpn'].cmd('xterm -e gdb -ex run -ex bt --args picoquicvpn {} -p 4443 2>&1 > log_server.log &'.format(plugins))
        else:
            net['vpn'].cmd('./picoquicvpn {} -p 4443 2>&1 > log_server.log &'.format(plugins))
        sleep(1)

    if tcpdump:
        if quic_tun:
            net['cl'].cmd('tcpdump -i tun0 -w tun0.pcap &')
        sleep(1)

    if quic_tun:
        if gdb:
            net['cl'].cmd('xterm -e gdb -ex run -ex bt --args picoquicvpn {} 10.0.3.1 4443 2>&1 > log_client.log &'.format(plugins))
        else:
            net['cl'].cmd('./picoquicvpn {} 10.0.3.1 4443 2>&1 > log_client.log &'.format(plugins))

    sleep(1)


def teardown_net(net):
    net['vpn'].cmd('pkill tcpdump')
    net['vpn'].cmd('pkill picoquicpvn')
    net['vpn'].cmd('pkill gdb')
    net['vpn'].cmd('pkill xterm')

    net['cl'].cmd('pkill tcpdump')
    net['cl'].cmd('pkill picoquicpvn')
    net['cl'].cmd('pkill gdb')
    net['cl'].cmd('pkill xterm')


def run():

    #Name of output file
    file_path = 'out.log';

    #Remove old log file
    if os.path.exists(file_path):
        os.remove(file_path)
    
    #x-axis label
    var_name = "sym_bw"
    #x-axis range
    var_range = [5, 300]
    #x-axis step range
    var_step_length = 50
    #Samples for each point
    repetitions = 5

    for i in range(var_range[0], var_range[1], var_step_length):
        for j in range(repetitions):

            #Append log to file
            with open(file_path, "a") as myfile:
                myfile.write(var_name + "|" + str(i) + "\n")

            #Execute one mininet configuration
            net_cleanup()
            net = Mininet(KiteTopo(bw_a=i, bw_b=i, delay_ms_a=0, delay_ms_b=0, loss_a=0, loss_b=0, queue_size_a=1000, queue_size_b=1000), link=TCLink, autoStaticArp=True, switch=OVSBridge, controller=None)
            net.start()
            setup_net(net, ip_tun=True, quic_tun=True, gdb=False, tcpdump=False, multipath=True)
            CLI(net, script="../scripts/mininet/auto_iperf.sh")
            teardown_net(net)
            net.stop()

    #Clean up multinet
    net_cleanup()
    
    #Concatenate name, generate diagram
    name = var_name + str(var_range[0]) + "_" + str(var_range[1]) + "_" + str(random.randint(5,10000))
    print("Filename: " + name)
    os.system("python3 generate_diagram.py " + name)
    #--OBS-- Only on systems with GUI
    os.system("xdg-open " + name + ".jpg &")

if __name__ == '__main__':
    setLogLevel('info')
    run()
