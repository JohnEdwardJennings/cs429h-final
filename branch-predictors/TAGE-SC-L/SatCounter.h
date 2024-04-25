#pragma once // somehow need to fix

#include <iostream>

template<int NUM_BITS>
class SatCounter {
public:
    static const int maximum = (1 << NUM_BITS) - 1;  // Maximum value the counter can hold
    int val;  // Current value of the counter
    // Constructor
    //SatCounter() : val(((1 << NUM_BITS) - 1) / 2) {}

    // Increment the counter
    void increment() {
        if (val < maximum) {
            val++;
        }
    }

    // Decrement the counter
    void decrement() {
        if (val > 0) {
            val--;
        }
    }

    void update(uint8_t taken) {
        if (taken) {
            increment();
        } else {
            decrement();
        }
    }

    // Get the current value of the counter
    int value() {
        return val;
    }

    bool predictTaken() {
        return val >= (maximum / 2);
    }

    // assume "not weak" means that only LSB can be 0 (ie 110 and 111 are "not weak")
    bool notWeak() {
        if (maximum <= 3) {
            return val == 0 || val == maximum; 
        }

        return val == 0 || val == 1 || val == maximum || val == maximum - 1; 
    }

    // Reset the counter to its initial state
    void reset() {
        val = 0;
    }

    // for debugging
    void print() {
        std::cout << "Counter Value: " << val << std::endl;
    }
};