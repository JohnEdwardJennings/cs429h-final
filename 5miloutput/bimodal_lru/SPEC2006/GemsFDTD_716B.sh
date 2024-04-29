#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/bimodal_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/bimodal_lru/SPEC2006/GemsFDTD_716B.json \
    /scratch/cluster/hill/traces/spec2006/GemsFDTD_716B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/bimodal_lru/SPEC2006/GemsFDTD_716B.txt
