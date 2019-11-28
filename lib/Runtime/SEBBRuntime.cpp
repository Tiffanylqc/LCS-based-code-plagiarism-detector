#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {

#define SEBB(X) SEBB_RUNTIME_##X

// extern uint64_t SEBB(numBBs);

static const char* kLogPath = "/tmp/ppa_detector_log";

constexpr uint32_t kBufferSize = 4 * 1024 * 1024;
constexpr uint64_t kLogDelimiter = 0xFFFFFFFFFFFFFFFF;
constexpr uint64_t kEnterBasicBlock = 0xFFFFFFFFFFFFFFFE;
constexpr uint64_t kExitBasicBlock = 0xFFFFFFFFFFFFFFFD;
constexpr uint64_t kInputMarker = 0x0000000000000000;
constexpr uint64_t kOutputMarker = 0x4000000000000000;

static uint64_t* SEBB(buffer) = nullptr;
static int fd = 0;
static uint32_t pos = 0;

static inline uint64_t im(uint64_t val) { return val | kInputMarker; }

static inline uint64_t om(uint64_t val) { return val | kOutputMarker; }

static inline void dumpToLogBuffer(uint64_t op, uint64_t val) {
  SEBB(buffer)[pos++] = op;
  SEBB(buffer)[pos++] = val;
}

void SEBB(init)() {
  fd = open(kLogPath, O_RDWR | O_CREAT, 0666);
  SEBB(buffer) = static_cast<uint64_t*>(
      mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
#ifdef VERBOSELOGGING
  printf("Running\n");
#endif
}

void SEBB(finalize)() {
  SEBB(buffer)[pos] = kLogDelimiter;
  msync(SEBB(buffer), kBufferSize, MS_SYNC);
  munmap(SEBB(buffer), kBufferSize);
  close(fd);
#ifdef VERBOSELOGGING
  printf("Exiting\n");
#endif
}

void SEBB(enter)(uint64_t id) {
  dumpToLogBuffer(kEnterBasicBlock, id);
#ifdef VERBOSELOGGING
  printf("Entering basic block #%lu\n", id);
#endif
}

void SEBB(exit)(uint64_t id) {
  dumpToLogBuffer(kExitBasicBlock, id);
#ifdef VERBOSELOGGING
  printf("Exiting basic block #%lu\n", id);
#endif
}

void SEBB(logInput)(uint64_t id, uint64_t val) {
  dumpToLogBuffer(im(id), val);
#ifdef VERBOSELOGGING
  printf("Basic block %lu has a new input of value %lu\n", id, val);
#endif
}

void SEBB(logOutput)(uint64_t id, uint64_t val) {
  dumpToLogBuffer(om(id), val);
#ifdef VERBOSELOGGING
  printf("Basic block %lu has a new output of value %lu\n", id, val);
#endif
}
}