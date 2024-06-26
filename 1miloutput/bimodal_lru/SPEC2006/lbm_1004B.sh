#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/bimodal_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/bimodal_lru/SPEC2006/lbm_1004B.json \
    /scratch/cluster/hill/traces/spec2006/lbm_1004B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/bimodal_lru/SPEC2006/lbm_1004B.txt

