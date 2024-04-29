#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/Tage_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/Tage_lru/SPEC2006/tonto_422B.json \
    /scratch/cluster/hill/traces/spec2006/tonto_422B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/Tage_lru/SPEC2006/tonto_422B.txt

