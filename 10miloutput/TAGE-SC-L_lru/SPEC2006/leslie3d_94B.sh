#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/TAGE-SC-L_lru_1_core_champsim \
    --warmup-instructions 10000000 \
    --simulation-instructions 10000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/TAGE-SC-L_lru/SPEC2006/leslie3d_94B.json \
    /scratch/cluster/hill/traces/spec2006/leslie3d_94B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/TAGE-SC-L_lru/SPEC2006/leslie3d_94B.txt

