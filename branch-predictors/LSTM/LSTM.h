#ifndef PRACTICE_H
#define PRACTICE_H

#include <string>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "clstm.h"
#include "extras.h"
#include "utils.h"


// Assuming 'net' is a class or struct, you would need to include it here.
// If it's defined in another header file, you would do something like:
// #include "net.h"

void gentest(Sequence &xs, Sequence &ys);
Float maxerr(Sequence &xs, Sequence &ys);
double test_net(Network net);


#endif // PRACTICE_H