#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/fourier_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/fourier_lru/SPEC2006/astar_23B.json \
    /scratch/cluster/hill/traces/spec2006/astar_23B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/fourier_lru/SPEC2006/astar_23B.txt

