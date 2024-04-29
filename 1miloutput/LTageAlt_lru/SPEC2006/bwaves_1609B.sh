#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/LTageAlt_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/LTageAlt_lru/SPEC2006/bwaves_1609B.json \
    /scratch/cluster/hill/traces/spec2006/bwaves_1609B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/LTageAlt_lru/SPEC2006/bwaves_1609B.txt
