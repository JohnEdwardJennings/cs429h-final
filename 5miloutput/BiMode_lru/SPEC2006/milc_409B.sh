#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/BiMode_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/BiMode_lru/SPEC2006/milc_409B.json \
    /scratch/cluster/hill/traces/spec2006/milc_409B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/BiMode_lru/SPEC2006/milc_409B.txt

