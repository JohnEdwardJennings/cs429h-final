#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define WAVE_BITS 4
#define LINE_BITS 9

namespace 
{
const int WAVE_COUNT = 1 << WAVE_BITS;
const int LINE_COUNT = 1 << LINE_BITS;

typedef struct Wave {
	double amplitude;
	uint8_t length;
	uint8_t counter;
	bool valid;
} Wave;

typedef struct Line {
	uint64_t ip_tag;
	Wave waves[WAVE_COUNT];
} Line;

Line bank[LINE_COUNT];

static uint64_t hash(uint64_t ip) { // gshare-like hash function 
	return (ip & ((1 << LINE_BITS) - 1)) ^ ((ip >> LINE_BITS) & ((1 << LINE_BITS) - 1)) ^ ((ip >> (2 * LINE_BITS)) & ((1 << LINE_BITS) - 1));	
}
}

void initialize_branch_predictor() {
	for (int i = 0; i < ::LINE_COUNT; i++) {
		::Line* line = &::bank[i];
		for (int j = 0; j < ::WAVE_COUNT; j++) {
			::Wave* wave = &(line->waves[j]);
			wave->amplitude = 0;
			wave->length = j + 1;
			wave->counter = 0;
			wave->valid = false;
		}	
	}
}

uint8_t predict_branch(uint64_t ip) {
	::Line* line = &::bank[hash(ip)];
	if (line->ip_tag != ip) {
		line->ip_tag = ip;
		for (int j = 0; j < ::WAVE_COUNT; j++) {
                        ::Wave* wave = &(line->waves[j]);
                        wave->amplitude = 0;
                        wave->length = j + 1;
                        wave->counter = 0;
                        wave->valid = false;
                }
		return false;
	}
	double branch_value = 0;
	for (int j = 0; j < ::WAVE_COUNT; j++) {
		::Wave* wave = &(line->waves[j]);
		branch_value += wave->valid * cos(2.0 * M_PI * wave->counter / wave->length) * wave->amplitude;
	}
	printf("%f, %d\n", branch_value, branch_value > 0);
	return (branch_value > 0);
}

void last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
	::Line* line = &::bank[hash(ip)];
	double branch_value = taken ? 1 : -1;
	for (int j = 0; j < WAVE_COUNT; j++) {
		::Wave* wave = &(line->waves[j]);
		wave->amplitude += branch_value * cos(2.0 * M_PI * wave->counter / wave->length);
		wave->counter = (wave->counter == 0) ? (wave->length - 1) : (wave->counter - 1);
		if (wave->counter == 0) {
			wave->valid = true;
		}
	}
}

int main() {
	int c = 0;
	initialize_branch_predictor();
	for (int i = 0; i < 1000; i++) {
		bool p = predict_branch(0);
		last_branch_result(0, 0, i % 5, 0);
		if (p == (i % 5 ? 1 : 0)) {
			c++;
		}
	}
	printf("%d\n", c);
	::Line* line = &::bank[0];
	for (int j = 0; j < WAVE_COUNT; j++) {
		::Wave* wave = &(line->waves[j]);
		printf("Amplitude at wavelength %d: %f\n", wave->length, wave->amplitude);
	}
	return 0;
}
