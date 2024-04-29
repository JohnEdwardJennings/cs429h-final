#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/hashed-perceptron_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/hashed-perceptron_lru/SPEC2006/sphinx3_883B.json \
    /scratch/cluster/hill/traces/spec2006/sphinx3_883B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/hashed-perceptron_lru/SPEC2006/sphinx3_883B.txt

