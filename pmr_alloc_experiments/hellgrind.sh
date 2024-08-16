#!/bin/bash

make all
# valgrind -s --tool=helgrind  build/main
valgrind -s --tool=drd  build/main
