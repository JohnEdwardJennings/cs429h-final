#pragma once

#include <iostream>
#include <vector>

namespace {
    const uint64_t LOG_ENTRIES = 6; // can change if necessary
    const uint64_t ENTRIES_PER_ROW = 4; // 4-way cache
    const uint64_t ENTRIES = 1 << LOG_ENTRIES;
    const uint64_t TAG_LENGTH = 10; // can change if necessary
    const uint64_t LOG_MAX_ITERATIONS = 10;
    const uint64_t MAX_ITERATIONS = 1 << LOG_MAX_ITERATIONS; // maximum iterations a loop can have to be predicted
    const uint64_t CONFIDENCE = 7; // whole loop must execute 7 times correctly in order to be eligible for prediction
    const uint64_t AGE_BITS = 4;
    const uint64_t AGE = ((1 << AGE_BITS) - 1) / 2; // default value of "age"
}

// for TAGE-SC-L predictor
class LoopPredictor {
public:
    struct LoopEntry {
        uint64_t pastIterationCount; // 10 bits
        uint64_t currIterationCount; // 10 bits
        uint64_t tag; // 10 bits
        uint64_t confidence; // 4 bits
        uint64_t age; // 4 bits
    };

    bool validLoopPredictionOccured;
    bool loopTaken;
    int colUsedForPrediction;
    LoopEntry loopPredTable[::ENTRIES][::ENTRIES_PER_ROW];// 4-way skewed associative table

    void resetEntry(std::size_t predInd, int col) {
        loopPredTable[predInd][col].age = 0;
        loopPredTable[predInd][col].tag = 0;
        loopPredTable[predInd][col].confidence = 0;
        loopPredTable[predInd][col].pastIterationCount = 0;
        loopPredTable[predInd][col].currIterationCount = 0;
    }

    void initializeLoopPredictor()
    {
        validLoopPredictionOccured = false;
        loopTaken = false;
        colUsedForPrediction = -1;
        for (std::size_t i = 0; i < ENTRIES; i++)
        {
            for (std::size_t j = 0; j < ENTRIES_PER_ROW; j++) {
                loopPredTable[i][j].pastIterationCount = 0;
                loopPredTable[i][j].currIterationCount = 0;
                loopPredTable[i][j].tag = 0;
                loopPredTable[i][j].age = 0;
                loopPredTable[i][j].confidence = 0;
            }
        }
    }

    // Predict whether the current loop iteration matches past behavior; based off TAGE-SC-L paper: 
    // https://inria.hal.science/hal-01086920/document
    bool predict(uint64_t address) {
        std::size_t predInd = address & ((1 << LOG_ENTRIES) - 1); // extracts rightmost bits
        std::size_t tag = (address >> LOG_ENTRIES) & ((1 << TAG_LENGTH) - 1); // to extract different bits for tag
        for (std::size_t col = 0; col < ::ENTRIES_PER_ROW; col++) {
            LoopEntry entry = loopPredTable[predInd][col];

            if (entry.tag == tag) {
                colUsedForPrediction = col;
                // check if entry is "strongly confident"; only then take the prediction seriously
                validLoopPredictionOccured = entry.confidence == ::CONFIDENCE;
                if (entry.pastIterationCount <= entry.currIterationCount + 1) {
                    loopTaken = false;
                    return false;
                } else {
                    loopTaken = true;
                    return true;
                }
            }
        }
        validLoopPredictionOccured = false; // loop predictor does not predict
        return false; 
    }

    // Update the predictor with the outcome of the loop
    void update(uint64_t address, uint8_t taken, uint8_t actualPrediction) {
        std::size_t predInd = address & ((1 << LOG_ENTRIES) - 1); // extracts rightmost bits

        if (colUsedForPrediction > -1) { // means a cache entry matches address's tag and index
            if (validLoopPredictionOccured && taken != loopTaken) { // wrong prediction used
                resetEntry(predInd,colUsedForPrediction);
                return;
                
            } else { // correct loop prediction
                if (loopPredTable[predInd][colUsedForPrediction].age < ::AGE * 2 + 1) {
                    loopPredTable[predInd][colUsedForPrediction].age++;
                }
            }
        
            loopPredTable[predInd][colUsedForPrediction].currIterationCount = 
                    (loopPredTable[predInd][colUsedForPrediction].currIterationCount + 1) % ::MAX_ITERATIONS;

            if (loopPredTable[predInd][colUsedForPrediction].currIterationCount 
                    > loopPredTable[predInd][colUsedForPrediction].pastIterationCount) {
                loopPredTable[predInd][colUsedForPrediction].confidence = 0; // don't predict until you know the correct "pastIterationCount"

                if (loopPredTable[predInd][colUsedForPrediction].pastIterationCount != 0) { // meaning this loop should terminate...so reset entry
                    resetEntry(predInd, colUsedForPrediction);
                }
            }

            if (!taken) {
                if (loopPredTable[predInd][colUsedForPrediction].currIterationCount == loopPredTable[predInd][colUsedForPrediction].pastIterationCount)
                {
                    // Increase the confidence since loop correctly predicted not taken after one whole execution
                    if (loopPredTable[predInd][colUsedForPrediction].confidence < ::CONFIDENCE) {
                        loopPredTable[predInd][colUsedForPrediction].confidence++;
                    }

                    // remove loops with less than CONFIDENCE iterations
                    if ((loopPredTable[predInd][colUsedForPrediction].pastIterationCount > 0) 
                            && (loopPredTable[predInd][colUsedForPrediction].pastIterationCount < ::CONFIDENCE))
                    {
                        resetEntry(predInd, colUsedForPrediction);
                    }
                }
                else {
                    // Set the actual iteration count of the newly allocated entry
                    if (loopPredTable[predInd][colUsedForPrediction].pastIterationCount == 0) {
                        loopPredTable[predInd][colUsedForPrediction].confidence = 0;
                        loopPredTable[predInd][colUsedForPrediction].pastIterationCount = 
                                loopPredTable[predInd][colUsedForPrediction].currIterationCount;
                    }
                    else {
                        resetEntry(predInd, colUsedForPrediction); // incorrect prediction because loop should have been taken
                    }
                }
                loopPredTable[predInd][colUsedForPrediction].currIterationCount = 0;
            }
        }
        else if (taken) { // If the branch is taken but no corresponding entry, must allocate a new entry
            size_t tag = (address >> LOG_ENTRIES) & ((1 << TAG_LENGTH) - 1); // to extract different bits for tag
            for (std::size_t i = 0; i < ENTRIES_PER_ROW; i++)
            {
                if (loopPredTable[predInd][i].age == 0) // meaning we can replace this entry
                {
                    loopPredTable[predInd][i].tag = tag;
                    loopPredTable[predInd][i].pastIterationCount = 0;
                    loopPredTable[predInd][i].currIterationCount = 1;
                    loopPredTable[predInd][i].age = AGE;
                    loopPredTable[predInd][i].confidence = 0;
                    break;
                }
                else if (loopPredTable[predInd][i].age > 0) {
                    loopPredTable[predInd][i].age--;
                }
            }
        }
    }

};