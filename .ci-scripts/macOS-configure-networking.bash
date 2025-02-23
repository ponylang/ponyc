#!/bin/bash

# Why?
# See: https://web.archive.org/web/20180129235834/http://danielmendel.github.io/blog/2013/04/07/benchmarkers-beware-the-ephemeral-port-limit/

sudo sysctl -w net.inet.tcp.msl=1000
sudo sysctl -w net.inet.ip.portrange.first=32768
sudo sysctl -w net.inet.ip.portrange.hifirst=32768
