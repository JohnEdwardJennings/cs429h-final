#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/LSTM_lru_1_core_champsim \
    --warmup-instructions 10000000 \
    --simulation-instructions 10000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/LSTM_lru/SPEC2006/bzip2_281B.json \
    /scratch/cluster/hill/traces/spec2006/bzip2_281B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/LSTM_lru/SPEC2006/bzip2_281B.txt

