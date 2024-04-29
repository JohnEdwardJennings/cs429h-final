#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/localHistory_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/localHistory_lru/SPEC2006/libquantum_964B.json \
    /scratch/cluster/hill/traces/spec2006/libquantum_964B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/localHistory_lru/SPEC2006/libquantum_964B.txt
