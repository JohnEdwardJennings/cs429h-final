#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/gshare_lru_1_core_champsim \
    --warmup-instructions 1000000 \
    --simulation-instructions 1000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/gshare_lru/SPEC2006/omnetpp_340B.json \
    /scratch/cluster/hill/traces/spec2006/omnetpp_340B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/gshare_lru/SPEC2006/omnetpp_340B.txt

