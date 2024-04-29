#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/hashed-perceptron_lru_1_core_champsim \
    --warmup-instructions 10000000 \
    --simulation-instructions 10000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/hashed-perceptron_lru/SPEC2006/GemsFDTD_109B.json \
    /scratch/cluster/hill/traces/spec2006/GemsFDTD_109B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/hashed-perceptron_lru/SPEC2006/GemsFDTD_109B.txt

