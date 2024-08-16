#!/bin/bash

make clean
make all
sudo perf record --call-graph dwarf build/main
sudo hotspot
