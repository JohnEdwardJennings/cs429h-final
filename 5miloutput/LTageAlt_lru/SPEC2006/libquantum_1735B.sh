#!/bin/bash

stdbuf --output=L /u/sahilsc/destination_directory/ChampSim/bin/LTageAlt_lru_1_core_champsim \
    --warmup-instructions 5000000 \
    --simulation-instructions 5000000 \
    --json /u/sahilsc/destination_directory/ChampSim/output/LTageAlt_lru/SPEC2006/libquantum_1735B.json \
    /scratch/cluster/hill/traces/spec2006/libquantum_1735B.trace.xz \
    | tee /u/sahilsc/destination_directory/ChampSim/output/LTageAlt_lru/SPEC2006/libquantum_1735B.txt

