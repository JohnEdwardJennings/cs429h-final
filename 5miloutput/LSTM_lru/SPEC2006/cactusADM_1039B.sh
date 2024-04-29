#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/LSTM_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/LSTM_lru/SPEC2006/cactusADM_1039B.json \
    /scratch/cluster/hill/traces/spec2006/cactusADM_1039B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/LSTM_lru/SPEC2006/cactusADM_1039B.txt
