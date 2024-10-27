#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

make clean
make all
sudo perf record --call-graph dwarf build/main
sudo chmod 777 perf.data
sudo hotspot --exportTo hotspot_results.perfparser perf.data
# sudo hotspot
