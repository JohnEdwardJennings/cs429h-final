#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/perceptron_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/perceptron_lru/SPEC2006/hmmer_7B.json \
    /scratch/cluster/hill/traces/spec2006/hmmer_7B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/perceptron_lru/SPEC2006/hmmer_7B.txt

