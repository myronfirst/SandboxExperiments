#!/bin/bash

make all
sudo perf record --call-graph dwarf build/main
sudo hotspot
