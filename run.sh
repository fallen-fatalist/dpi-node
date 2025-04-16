#!/bin/bash
./build/basicfwd \
  --no-huge --no-pci -m 1024 \
  --vdev=net_af_packet0,iface=wlo1 \
  --vdev=net_af_packet1,iface=veth_tx 
