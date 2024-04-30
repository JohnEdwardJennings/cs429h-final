#include <assert.h>
#include <math.h>
#include <unistd.h>

#include <bitset>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

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

//network_detail prints out all relevant weights for the LSTM
namespace {
constexpr std::size_t NN_TABLE_SIZE = 16384;
constexpr std::size_t NN_PRIME = 16381;
constexpr int TIME_STEPS = 8;
constexpr int NN_HIST_LENGTH = TIME_STEPS;

std::array<bitset<NN_HIST_LENGTH>, NN_TABLE_SIZE> table;
std::array<Network, NN_TABLE_SIZE> branch_predictors;

} // namespace
int ntrain = 10000;
int ntest = 1000;

uint64_t hash_ip(uint64_t ip) { return ip % ::NN_PRIME; }

void convertInputBitsetToSequence(bitset<::NN_HIST_LENGTH> &bs, Sequence &seq) {
  for (int i = 0; i < ::TIME_STEPS; i++) {
    seq[i].v(0, 0) = bs[::TIME_STEPS - i - 1];
  }
}

// Generates all 0 bitset for all elements in table
void genStart() {
  for (std::size_t i = 0; i < NN_TABLE_SIZE; ++i) {
    table[i].reset();
  }
}
void initialize_branch_predictor() {
  genStart();
  int gpu = getienv("gpu", -1);
  for (std::size_t i = 0; i < NN_TABLE_SIZE; ++i) {
    branch_predictors[i] = make_net(
        "lstm1", {{"ninput", 1}, {"nhidden", 4}, {"noutput", 2}, {"gpu", gpu}});
    branch_predictors[i]->setLearningRate(5e-2, 0.9);
  }
}

uint8_t predict_branch(uint64_t ip) {
  uint64_t index = hash_ip(ip);
  Network &net = branch_predictors[index];

  // Prepare input sequence
  Sequence input_seq;
  input_seq.resize(::TIME_STEPS, 1, 1);
  input_seq.zero();
  convertInputBitsetToSequence(table[index], input_seq);

  // forward propagation
  set_inputs(net, input_seq);
  net->forward();

  float taken_prob = net->outputs[::TIME_STEPS - 1].v(0, 0);
  uint8_t prediction = taken_prob > 0.5;
  // cout << "Probability of Taking Branch: " << taken_prob << endl;
  // cout << "not_taken_prob: " <<  net->outputs[0].v(1, 0) << endl;
  // cout << "" <<  net->outputs[0].v(1, 0) << endl;
  return prediction;
}

void last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken,
                        uint8_t branch_type) {
  global_loop_counter += 1;
  uint64_t index = hash_ip(ip);
  Network &net = branch_predictors[index];

  Sequence target_seq;
  target_seq.resize(::TIME_STEPS, 2, 1);
  target_seq.zero();
  // target_seq[0].v(0, 0) = taken
  // std::cout  << "target_seq "  << endl;
  for (int t = 0; t < ::TIME_STEPS - 1; t++) {
    target_seq[t].v(0, 0) = table[index][::TIME_STEPS - t - 2];
    target_seq[t].v(1, 0) = 1 - table[index][::TIME_STEPS - t - 2];
    // cout <<  table[index][::TIME_STEPS - t - 2];
  }
  // int true_taken = taken > 0;
  target_seq[::TIME_STEPS - 1].v(0, 0) = taken > 0;
  target_seq[::TIME_STEPS - 1].v(1, 0) = 1 - (taken > 0);
  // cout << (taken > 0) << endl;
  set_targets(net, target_seq);
  net->backward();
  sgd_update(net);

  // cout << "prev: " << table[index] << " taken: " << (taken > 0);
  // update bitset
  table[index] <<= 1;
  table[index][0] = taken > 0;
  // cout << " after: " << table[index] << endl;
  int stepsToPrint = 10000;
  if (global_loop_counter % 1000 == 0) {
      network_detail(net);
    }
}

void gentest(Sequence &xs, Sequence &ys) {
  int N = 20;
  xs.resize(N, 1, 1);
  xs.zero();
  ys.resize(N, 2, 1);
  ys.zero();
  ys[0].v(0, 0) = 1;
  int random_start = rand() % 4;
  for (int t = 0; t < N; t++) {
    int out = (drand48() < 0.3);
    xs[t].v(0, 0) = out;
    if (t < N - 1)
      ys[t + 1].v(out, 0) = 1.0;
  }
}

Float maxerr(Sequence &xs, Sequence &ys) {
  Float merr = 0.0;
  for (int t = 0; t < xs.size(); t++) {
    for (int i = 0; i < xs.rows(); i++) {
      for (int j = 0; j < ys.cols(); j++) {
        Float err = fabs(xs[t].v(i, j) - ys[t].v(i, j));
        merr = fmax(err, merr);
      }
    }
  }
  return merr;
}

//testing LSTM is able to learn a simple sequence
double test_net(Network net) {
  Float merr = 0.0;
  for (int i = 0; i < ntest; i++) {
    Sequence xs, ys;
    gentest(xs, ys);
    set_inputs(net, xs);
    net->forward();
    if (getienv("verbose", 0)) {
      for (int t = 0; t < xs.size(); t++)
        cout << xs[t].v(0, 0);
      cout << endl;
      for (int t = 0; t < net->outputs.size(); t++) {

        cout << int(0.5 + net->outputs[t].v(1, 0));
      }
      cout << endl;
      cout << endl;
    }

    Float err = maxerr(net->outputs, ys);
    if (err > merr)
      merr = err;
  }
  return merr;
}

//Initial test that my LSTM actually is able to learn anything at all (not specific to branch prediction)
void test1() {
  initialize_branch_predictor();
  int loops = 10000;
  // keep track of accuracy over past 100 entries
  // double merr = 0.0;
  double ones = 0;
  int keepTrackOfRunningAccuracyLength = 100;
  double overAllAccuracy = 0.0;
  int stepBetweenPrints = 10;
  std::queue<bool> history;
  for (int i = 0; i < keepTrackOfRunningAccuracyLength; i++) {
    history.push(0);
  }
  double accuracy = 0;
  int checkPoint = 0;
  bool gotFirst = 0;
  for (int i = 0; i < loops; i++) {
    uint64_t ip = 0;
    int taken = i % 4 == 0;
    int predicted = static_cast<int>(predict_branch(ip));
    bool correct = predicted == taken;
    // cout << "correct: " << correct << " predicted: "
    //   << predicted << " taken: " << taken << " i: " << i << std::endl;
    history.push(correct);
    ones += -1 * history.front() + correct;
    history.pop();
    last_branch_result(ip, 10, taken, 0);
    accuracy = ones * 1.0 / keepTrackOfRunningAccuracyLength;
    overAllAccuracy += 1;
    gotFirst = (accuracy - 1 < 0.0001) && (1 - accuracy < 0.0001);
    // if (!gotFirst) {
    // cout << "Accuracy Over The Past 100 Trials: " << accuracy << endl;
    //   cout << "Loop iteration: " << i << " Accuracy: " << accuracy << std::endl;
    //   checkPoint = i;

    // } else {
    //     cout << "Learned loop at " << checkPoint;
    // }

  }

}
void test2() {
  cout << "test-lstm" << endl;
  Network net;
  int gpu = getienv("gpu", -1);
  net = make_net("lstm1",
                 {{"ninput", 1}, {"nhidden", 4}, {"noutput", 2}, {"gpu", gpu}});
  net->setLearningRate(1e-4, 0.9);
  print("training 1:4:2 network to learn delay");
  vector<float> states;
  for (int i = 0; i < ntrain; i++) {
    Sequence xs, ys;
    gentest(xs, ys);
    set_inputs(net, xs);
    net->forward();
    set_targets(net, ys);
    net->backward();
    sgd_update(net);
  }
  network_detail(net);
  double merr0 = test_net(net);
  if (merr0 > 0.1) {
    print("FAILED (pre-save)", merr0);
    exit(1);
  } else {
    print("OK (pre-save)", merr0);
  }
  double merr = test_net(net);
  if (merr > 0.1) {
    print("FAILED", merr);
    exit(1);
  } else {
    print("OK", merr);
  }
  int nparams = n_params(net);
  assert(nparams > 0);
  print("nparams", nparams);
  vector<float> params(nparams);
  vector<float> backup;
  get_params(net, &params[0], nparams);
  backup = params;
  share_params(net, &params[0], nparams);
  double merr2 = test_net(net);
  if (merr2 > 0.1) {
    print("FAILED (params)", merr2);
    exit(1);
  } else {
    print("OK (params)", merr2);
  }
  for (int i = 0; i < nparams; i++)
    params[i] = 0.0;
  double merr3 = test_net(net);
  if (merr3 < 0.1) {
    print("FAILED (hacked-params)", merr3);
    exit(1);
  } else {
    print("OK (hacked-params)", merr3);
  }
  for (int i = 0; i < nparams; i++)
    params[i] = backup[i];
  double merr4 = test_net(net);
  if (merr4 > 0.1) {
    print("FAILED (restored-params)", merr4);
    exit(1);
  } else {
    print("OK (restored-params)", merr4);
  }
}

int main(int argc, char **argv) {
  test1();
  // test2();
}
