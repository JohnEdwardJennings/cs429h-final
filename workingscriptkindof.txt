#!/bin/bash

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Helper function to extract prediction accuracy and MPKI from a file
extract_stats() {
    local file="$1"
    local accuracy=$(grep "Branch Prediction Accuracy" "$file" | awk '{print $6}' | tr -d '%')
    local mpki=$(grep "Branch Prediction Accuracy" "$file" | awk '{print $8}')
    echo "$accuracy|$mpki"
}

# Function to print branch prediction accuracy for a single branch predictor
print_single_predictor() {
    local predictor="$1"
    echo "***"
    echo "$predictor: Branch Prediction Accuracy | MPKI | and so on"
    for file in "$SCRIPT_DIR/out/"${predictor}*out; do
        stats=$(extract_stats "$file")
        accuracy=$(cut -d'|' -f1 <<< "$stats")
        mpki=$(cut -d'|' -f2 <<< "$stats")
        printf "%s: %.2f%% | %.2f\n" "${file##*/}" "$accuracy" "$mpki"
    done
    echo "***"
}

# Function to print branch prediction accuracy for all branch predictors
# Function to print branch prediction accuracy for all branch predictors
print_all_predictors() {
    echo "***"
    echo "bimodal-copy, gshare-copy, perceptron-copy: Branch Prediction Accuracy | MPKI | and so on"
    for file in "$SCRIPT_DIR/out/"*out; do
        stats=$(extract_stats "$file")
        bimodal_stats=$(extract_stats "$file")
        gshare_stats=$(extract_stats "${file/bimodal-copy/gshare-copy}")
        perceptron_stats=$(extract_stats "${file/bimodal-copy/perceptron-copy}")
        accuracies=$(printf "%s, %s, %s" "${bimodal_stats%|*}" "${gshare_stats%|*}" "${perceptron_stats%|*}" | sed 's/|/%, /g')
        mpkis=$(printf "%s, %s, %s" "${bimodal_stats#*|}" "${gshare_stats#*|}" "${perceptron_stats#*|}" | sed 's/|/, /g')
        echo "${file##*/}: $accuracies | $mpkis"
    done
    echo "***"
}

# Function to print mean stats across all branch predictors
print_mean_stats() {
    echo "***"
    echo "Prediction accuracy | MPKI | IPC | ... and so on"
    for predictor in bimodal-copy gshare-copy perceptron-copy; do
        accuracies=()
        mpkis=()
        for file in "$SCRIPT_DIR/out/"${predictor}*out; do
            stats=$(extract_stats "$file")
            accuracies+=("$(cut -d'|' -f1 <<< "$stats")")
            mpkis+=("$(cut -d'|' -f2 <<< "$stats")")
        done
        mean_accuracy=$(printf '%.2f' $(printf '%s\n' "${accuracies[@]}" | awk '{sum+=$1} END {print sum/NR}'))
        mean_mpki=$(printf '%.2f' $(printf '%s\n' "${mpkis[@]}" | awk '{sum+=$1} END {print sum/NR}'))
        echo "$predictor $mean_accuracy% $mean_mpki"
    done
    echo "***"
}

# Function to print branch prediction accuracy only
print_accuracy_only() {
    local predictor="$1"
    for file in "$SCRIPT_DIR/out/"${predictor}*out; do
        accuracy=$(grep "Branch Prediction Accuracy" "$file" | awk '{print $5}')
        echo "${file##*/}: $accuracy"
    done
}

# Main script
if [ "$1" == "--accuracy-only" ]; then
    print_accuracy_only "bimodal-copy"
    print_accuracy_only "gshare-copy"
    print_accuracy_only "perceptron-copy"
else
    print_single_predictor "bimodal-copy"
    print_all_predictors
    print_mean_stats
fi
