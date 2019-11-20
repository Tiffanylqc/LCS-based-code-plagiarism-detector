#include <cstdint>
#include <cstdio>

extern "C" {

#define SEBB(X) SEBB_RUNTIME_##X

// extern uint64_t SEBB(numBBs);

void SEBB(init)() { printf("Running\n"); }

void SEBB(finalize)() { printf("Exiting\n"); }

void SEBB(enter)(uint64_t id) { printf("Entering basic block #%lu\n", id); }

void SEBB(exit)(uint64_t id) { printf("Exiting basic block #%lu\n", id); }
}