#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/BiMode_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/BiMode_lru/SPEC2006/soplex_66B.json \
    /scratch/cluster/hill/traces/spec2006/soplex_66B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/BiMode_lru/SPEC2006/soplex_66B.txt

