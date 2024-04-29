#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"
#include <array>
#include <bitset>
#include <assert.h>
#include <math.h>
#include <unistd.h>

#include <bitset>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <queue>

#include "clstm.h"
#include "extras.h"
#include "utils.h"


int global_loop_counter = 0;

using std_string = std::string;
#define string std_string
using std::cout;
using std::make_pair;
using std::shared_ptr;
using std::stoi;
using std::to_string;
using std::unique_ptr;
using std::vector;
using namespace Eigen;
using namespace ocropus;


//This is the LSTM branch predictor
//It uses a LSTM network to predict the branch outcome by using a LSTM per PC
//The LSTM is trained using the last 8 branch outcomes
//Many commented out lines are used in the code to print debug information
namespace
{
constexpr std::size_t NN_TABLE_SIZE = 16384;
constexpr std::size_t NN_PRIME = 16381;
constexpr int TIME_STEPS = 8;
constexpr int NN_HIST_LENGTH = TIME_STEPS;

std::map<O3_CPU*, std::array<bitset<NN_HIST_LENGTH>, NN_TABLE_SIZE>> table;
std::map<O3_CPU*, std::array<Network, NN_TABLE_SIZE>> branch_predictors;
} // namespace


uint64_t hash_ip(uint64_t ip) {
    return ip % ::NN_PRIME;
}

void convertInputBitsetToSequence(bitset<::NN_HIST_LENGTH> &bs, Sequence &seq) {
    for (int i = 0; i < ::TIME_STEPS; i++) {
        seq[i].v(0, 0) = bs[::TIME_STEPS - i - 1];
    }
    // cout << "input seq" << bs << endl;
}

// Generates all 0 bitset for all elements in ::table[this]
void genStart() {
    for (std::size_t i = 0; i < NN_TABLE_SIZE; ++i) {
        table[i].reset();
    }
}

void O3_CPU::initialize_branch_predictor() {
    int gpu = getienv("gpu", -1);
    for (std::size_t i = 0; i < NN_TABLE_SIZE; ++i) {
        branch_predictors[this][i] = make_net("lstm1",
                                        {{"ninput", 1}, {"nhidden", 4}, {"noutput", 2}, {"gpu", gpu}});
        branch_predictors[this][i]->setLearningRate(1e-4, 0.9);
    }
}

uint8_t O3_CPU::predict_branch(uint64_t ip) {
    uint64_t index = hash_ip(ip);
    Network &net = branch_predictors[this][index];

    // Prepare input sequence. The last element is the current branch. 
    // Takes in the previous TIME_STEPS branch history
    Sequence input_seq;
    input_seq.resize(::TIME_STEPS, 1, 1);
    input_seq.zero();
    convertInputBitsetToSequence(::table[this][index], input_seq);

    // forward propagation
    set_inputs(net, input_seq);
    net->forward();

    float taken_prob = net->outputs[::TIME_STEPS - 1].v(0, 0);
    uint8_t prediction = taken_prob > 0.5;
    // cout << "Probability of Taking: " << taken_prob << endl;
    // cout << "not_taken_prob: " <<  net->outputs[0].v(1, 0) << endl;
    return prediction;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    global_loop_counter += 1;
    uint64_t index = hash_ip(ip);
    Network &net = branch_predictors[this][index];

    Sequence target_seq;
    // Prepare target sequence. The last element is the true value current branch.
    //It outputs a 2x1 vector with the first element descri
    target_seq.resize(::TIME_STEPS, 2, 1);
    target_seq.zero();
    for (int t = 0; t < ::TIME_STEPS - 1; t++) {
        target_seq[t].v(0, 0) = ::table[this][index][::TIME_STEPS - t - 2];
        target_seq[t].v(1, 0) = 1-::table[this][index][::TIME_STEPS - t - 2];
    }
    target_seq[::TIME_STEPS - 1].v(0, 0) =taken > 0; 
    target_seq[::TIME_STEPS - 1].v(1, 0) =  1- (taken > 0); 
    set_targets(net, target_seq);
    net->backward();
    sgd_update(net);

    // update bitset
    ::table[this][index] <<= 1;
    ::table[this][index][0] = taken > 0;
    int stepsToPrint = 10000;
}