#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/perceptron_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/perceptron_lru/SPEC2006/soplex_205B.json \
    /scratch/cluster/hill/traces/spec2006/soplex_205B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/perceptron_lru/SPEC2006/soplex_205B.txt

