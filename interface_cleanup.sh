sudo ip link set virbr0 down

sudo ip tuntap del dev tap0 mode tap
sudo brctl delbr virbr0
