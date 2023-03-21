# Script to allow composite in qemu to access the internet from using the host's wireless interface

# create bridge and tap interfaces
sudo brctl addbr virbr0
sudo ip tuntap add dev tap0 mode tap

# add tap interface to the bridge
sudo brctl addif virbr0 tap0

# set hardware address of tap0
sudo ip link set address 10:10:10:10:10:11 dev tap0

# bring both interfaces up
sudo ip link set tap0 up
sudo ip link set virbr0 up

# assign ip to the bridge
sudo ip addr add 10.10.1.1/24 dev virbr0

# composite does not support ARP requests yet :(
sudo arp -s 10.10.1.2 66:66:66:66:66:66

# forward all traffic from bridge interface to the wireless interface...
sudo iptables -A FORWARD -i virbr0 -o wlo1 -j ACCEPT

# ...and the other way
sudo iptables -A FORWARD -i wlo1 -o virbr0 -j ACCEPT

# NAT all traffic out of wlo1
sudo iptables -t nat -A POSTROUTING -o wlo1 -j MASQUERADE

