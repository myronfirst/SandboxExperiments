#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

repetitions=30
make
for i in $(seq 1 $repetitions); do
    echo $i
    ./cache_warm
done
