#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/gshare_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/gshare_lru/SPEC2006/cactusADM_734B.json \
    /scratch/cluster/hill/traces/spec2006/cactusADM_734B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/gshare_lru/SPEC2006/cactusADM_734B.txt

