#!/bin/sh

sudo ip link set up dev $1
sudo ip addr add 192.168.20.1/24 dev $1