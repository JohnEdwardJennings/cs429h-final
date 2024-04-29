#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/gselect_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/gselect_lru/SPEC2006/leslie3d_94B.json \
    /scratch/cluster/hill/traces/spec2006/leslie3d_94B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/gselect_lru/SPEC2006/leslie3d_94B.txt

