#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/localHistory_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/localHistory_lru/SPEC2006/mcf_46B.json \
    /scratch/cluster/hill/traces/spec2006/mcf_46B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/localHistory_lru/SPEC2006/mcf_46B.txt

