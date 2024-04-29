#include <iostream>

namespace {
    const uint64_t SC_LOG_ENTRIES = 6; // can change if necessary
    const uint64_t SC_ENTRIES_PER_ROW = 2; // 2 way cache
    const uint64_t SC_ENTRIES = 1 << SC_LOG_ENTRIES;
    const uint64_t SC_TAG_LENGTH = 7; // can change if necessary
    const uint64_t STARTING_THRESHOLD = 0; // passing this threshold means SC will invert prediction

    uint8_t SC_prediction = 0;
}



class SCPredictor {
public:
    typedef struct SC_table_entry {
        uint8_t counter;  // 6 bit counter
        uint8_t tag;      // 7 bit tag for identifying the entry
        uint64_t threshold; // 3 bit threshold (will range from 10 - 17, so adding 10 to each value)
    } SC_table_entry;

    SC_table_entry SCtable[::SC_ENTRIES][::SC_ENTRIES_PER_ROW]; // 2 way associative cache

    void initialize_SC() {
        for (std::size_t i = 0; i < SC_ENTRIES; i++)
        {
            for (std::size_t j = 0; j < SC_ENTRIES_PER_ROW; j++) {
                SCtable[i][j].counter = 0;
                SCtable[i][j].tag = 0;
                SCtable[i][j].threshold = STARTING_THRESHOLD + 10;
            }
        }
        
    }

    // returns true if SC predicts to invert, false otherwise
    bool SC_predict(uint64_t PC, uint8_t TAGEdirection) {
        uint64_t hash = (PC ^ TAGEdirection) % SC_ENTRIES;
        uint8_t tag = (PC & 0b0000000000000000000000000000000000000000000000000000000000111111) & (TAGEdirection << 6);
        std::size_t curCol = 0;
        for (curCol = 0; curCol < SC_ENTRIES_PER_ROW; curCol++) {
            SC_table_entry curEntry = SCtable[hash][curCol];
            if (curEntry.tag == tag) {
                if (curEntry.counter > (curEntry.threshold + 10)) {
                    SC_prediction = 1;
                    return true;
                }
            }
        }

        SC_prediction = 0;
        return false;
    }

    void update(uint64_t PC, uint8_t TAGEprediction, uint8_t taken) {
        uint64_t hash = (PC ^ TAGEprediction) % SC_ENTRIES;
        uint8_t tag = (PC & 0b0000000000000000000000000000000000000000000000000000000000111111) & (TAGEprediction << 6);
        std::size_t curCol = 0;
        for (curCol = 0; curCol < SC_ENTRIES_PER_ROW; curCol++) {
            SC_table_entry curEntry = SCtable[hash][curCol];
            if (curEntry.tag == tag) {
                break;
            }
        }

        // nonexistent entry, need to make one (only if we mispredicted though)
        if ((curCol >= SC_ENTRIES_PER_ROW) && (TAGEprediction != taken)) {
            std::size_t curCol2 = 0;

            // check if theres an empty space
            for (curCol2 = 0; curCol2 < SC_ENTRIES_PER_ROW; curCol2++) {
                SC_table_entry curEntry = SCtable[hash][curCol2];
                if (curEntry.tag == 0) {
                    break;
                }
            }

            // there is an empty spot, put the new entry there
            if (curCol2 < SC_ENTRIES_PER_ROW) {
                SCtable[hash][curCol2].counter = 1;
                SCtable[hash][curCol2].tag = tag;
                SCtable[hash][curCol2].threshold = STARTING_THRESHOLD + 10;
            } else { // else replace the one farthest from threshold, then lowest counter, then highest threshold
                std::size_t replaceCol = 0;
                uint8_t replaceThresh = SCtable[hash][0].threshold;
                uint8_t replaceCounter = SCtable[hash][0].counter;

                // look for replace spot
                for (std::size_t i = 0; i < SC_ENTRIES_PER_ROW; i++) {
                    SC_table_entry curEntry = SCtable[hash][i];
                    uint8_t curDiff = (curEntry.threshold + 10) - curEntry.counter;
                    uint8_t replaceDiff = (replaceThresh + 10) - replaceCounter;
                    if (curDiff > replaceDiff) {
                        replaceCol = i;
                        replaceThresh = curEntry.threshold;
                        replaceCounter = curEntry.counter;
                    } else if ((curDiff == replaceDiff) && (curEntry.counter < replaceCounter)) {
                        replaceCol = i;
                        replaceThresh = curEntry.threshold;
                        replaceCounter = curEntry.counter;
                    } else if ((curDiff == replaceDiff) && (curEntry.threshold > replaceThresh)) {
                        replaceCol = i;
                        replaceThresh = curEntry.threshold;
                        replaceCounter = curEntry.counter;
                    }
                }

                SCtable[hash][replaceCol].counter = 1;
                SCtable[hash][replaceCol].tag = tag;
                SCtable[hash][replaceCol].threshold = STARTING_THRESHOLD + 10;
            }
        }

        // the entry exists
        if (curCol < SC_ENTRIES_PER_ROW) {
            // if TAGE was correct
            if (TAGEprediction == taken) {
                // SC didn't invert, so SC is correct too
                if (!SC_prediction) {
                    // if (SCtable[hash][curCol].threshold > 0) {
                    //     SCtable[hash][curCol].threshold--;
                    // }
                    if (SCtable[hash][curCol].counter > 0) {
                        SCtable[hash][curCol].counter--;
                    }
                } else { // SC mistakenly inverted
                    if (SCtable[hash][curCol].counter > 0) {
                        SCtable[hash][curCol].counter--;
                    }
                    if (SCtable[hash][curCol].threshold < 7) {
                        SCtable[hash][curCol].threshold++;
                    }
                }
            } else { // if TAGE was wrong
                if (!SC_prediction) { // SC didn't invert, when it shouldve
                    if (SCtable[hash][curCol].counter < 63) {
                        SCtable[hash][curCol].counter++;
                    } 
                    if (SCtable[hash][curCol].threshold > 0) {
                        SCtable[hash][curCol].threshold--;
                    }
                } else { // SC correctly inverted
                    if (SCtable[hash][curCol].counter < 63) {
                        SCtable[hash][curCol].counter++;
                    } 
                    // if (SCtable[hash][curCol].threshold > 0) {
                    //     SCtable[hash][curCol].threshold--;
                    // }
                }
            }
        }

    }
};
