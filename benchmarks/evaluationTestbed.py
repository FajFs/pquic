from time import sleep
import sys
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
        mqs = 1500#int(1.5 * (((opts['bw_a'] * 1000000) / 8) / 1500) * (2 * 70 / 1000.0))  # 1.5 * BDP, TODO: This assumes that packet size is 1500 bytes
        opt_path1 = {'delay': str(opts['delay_ms_a']) + "ms", 'loss': opts['loss_a'], 'bw': opts['bw_a'], 'max_queue_size': mqs}
        opt_path2 = {'delay': str(opts['delay_ms_b']) + "ms", 'loss': opts['loss_b'], 'bw': opts['bw_b'], 'max_queue_size': mqs}
        print(opt_path1)
        generic_opts = {'delay': '0ms', 'max_queue_size': 1500}
        self.r1 = self.addNode('r1', cls=LinuxRouter)
        self.r2 = self.addNode('r2', cls=LinuxRouter)
        self.r3 = self.addNode('r3', cls=LinuxRouter)

        self.cl = self.addHost('cl')
        self.vpn = self.addHost('vpn')

        self.addLink(self.cl, self.r1, intfName1='cl-eth0', intfName2='r1-eth0')
        self.addLink(self.cl, self.r2, intfName1='cl-eth1', intfName2='r2-eth0')
        self.addLink(self.r1, self.r3, intfName1='r1-eth1', intfName2='r3-eth1', **opt_path1)
        self.addLink(self.r2, self.r3, intfName1='r2-eth1', intfName2='r3-eth2', **opt_path2)
        self.addLink(self.r3, self.vpn,intfName1='r3-eth0', intfName2='vpn-eth0')



def setup_ips(net):
    config = {
        'r1': {
            'r1-eth0': '10.1.0.1/24',
            'r1-eth1': '10.2.0.1/24'
        },
        'r2': {
            'r2-eth0': '10.1.1.1/24',
            'r2-eth1': '10.2.1.1/24'
        },
        'r3': {
            'r3-eth0': '10.2.2.1/24',
            'r3-eth1': '10.2.0.2/24',
            'r3-eth2': '10.2.1.2/24',
        },
        'cl': {
            'cl-eth0': '10.1.0.2/24',
            'cl-eth1': '10.1.1.2/24',
            'default': 'via 10.1.0.1 dev cl-eth0'
        },
        'vpn': {
            'vpn-eth0': '10.2.2.2/24',
            'default': 'via 10.2.2.1 dev vpn-eth0'
        }
    }

    for h in config:
        for intf, ip in sorted(config[h].items(), key=lambda x: x[0] == 'default'):
            if intf != 'default':
                print net[h].cmd('ip addr flush dev {}'.format(intf))
                print net[h].cmd('ip addr add {} dev {}'.format(ip, intf))
            else:
                print net[h].cmd('ip route add default {}'.format(ip))


def setup_routes(net):
    vpn_addr = '10.2.2.2'
    cl_addr1 = '10.1.0.2'
    cl_addr2 = '10.1.1.2'

    print net['r1'].cmd('ip route add {} via 10.2.0.2 dev r1-eth1'.format(vpn_addr))

    print net['r2'].cmd('ip route add {} via 10.2.1.2 dev r2-eth1'.format(vpn_addr))

    print net['r3'].cmd('ip route add {} via 10.2.0.1 dev r3-eth1'.format(cl_addr1))
    print net['r3'].cmd('ip route add {} via 10.2.1.1 dev r3-eth2'.format(cl_addr2))


def setup_client_tun(nodes, id, queue_discipline):
    tun_addr = '10.4.0.2/24'
    node = nodes[id]

    print node.cmd('modprobe tun')
    print node.cmd('ip tuntap add mode tun dev tun0')
    print node.cmd('ip addr add {} dev tun0'.format(tun_addr))
    print node.cmd('ip link set dev tun0 mtu 1400')
    print node.cmd('ip link set dev tun0 up')

    print node.cmd('ip rule add from 10.1.0.2 table 1')
    print node.cmd('ip route add 10.1.0.0/24 dev cl-eth0 scope link table 1')
    print node.cmd('ip route add default via 10.1.0.1 dev cl-eth0 table 1')
    print node.cmd('ip rule add from 10.1.1.2 table 2')
    print node.cmd('ip route add 10.1.1.0/24 dev cl-eth1 scope link table 2')
    print node.cmd('ip route add default via 10.1.1.1 dev cl-eth1 table 2')

    if queue_discipline == "pfifo":
        print node.cmd('tc qdisc replace dev tun0 root pfifo')

    # The two following lines are very important!
    print node.cmd('ip route add default via 10.1.0.1 dev cl-eth0 metric 100')
    print node.cmd('ip route add default via 10.1.1.1 dev cl-eth1 metric 101')


def setup_server_tun(nodes, id, server_addr, queue_discipline):
    tun_addr = '10.4.0.1/24'
    node = nodes[id]

    print node.cmd('modprobe tun')
    print node.cmd('ip tuntap add mode tun dev tun1')
    print node.cmd('ip addr add {} dev tun1'.format(tun_addr))
    print node.cmd('ip link set dev tun1 mtu 1400')
    print node.cmd('ip link set dev tun1 up')
    if queue_discipline == "pfifo":
        print node.cmd('tc qdisc replace dev tun1 root pfifo')

    print node.cmd('sysctl net.ipv4.ip_forward=1')


def setup_net(net, ip_tun=True, quic_tun=True, gdb=False, tcpdump=False, multipath=False):
    setup_ips(net)
    setup_routes(net)

    vpn_addr = '10.2.2.2'

    if ip_tun:
        queue_discipline = sys.argv[1]
        setup_client_tun(net, 'cl', queue_discipline)
        setup_server_tun(net, 'vpn', vpn_addr, queue_discipline)

    net['cl'].cmd('ping -i 0.25 -I cl-eth0 -c 4 {}'.format(vpn_addr))
    net['cl'].cmd('ping -i 0.25 -I cl-eth1 -c 4 {}'.format(vpn_addr))
    net['vpn'].cmd('ping -i 0.25 -c 4 {}'.format('10.1.1.2'))
    net['vpn'].cmd('ping -i 0.25 -c 4 {}'.format('10.1.0.2'))

    plugins = " -P plugins/datagram/datagram.plugin"
    if multipath:
        plugins += " -P plugins/multipath/multipath_rr.plugin"

    if quic_tun:
        net['vpn'].cmd('./picoquicvpn {} -p 4443 2>&1 > log_server.log &'.format(plugins))
        sleep(1)

    if quic_tun:
        net['cl'].cmd('./picoquicvpn {} 10.2.2.2 4443 2>&1 > log_client.log &'.format(plugins))

    net['vpn'].cmd('netserver')
    sleep(1)


def teardown_net(net):
    net['vpn'].cmd('pkill picoquicpvn')
    net['cl'].cmd('pkill picoquicpvn')



def run():
    net_cleanup()
    net = Mininet(KiteTopo(bw_a=20, bw_b=20, delay_ms_a=10, delay_ms_b=10, loss_a=0, loss_b=0), link=TCLink, autoStaticArp=True, switch=OVSBridge, controller=None)
    net.start()
    setup_net(net, ip_tun=True, quic_tun=True, gdb=False, tcpdump=False, multipath=True)

    CLI(net)
    teardown_net(net)
    net.stop()
    net_cleanup()


if __name__ == '__main__':
    setLogLevel('info')
    run()
