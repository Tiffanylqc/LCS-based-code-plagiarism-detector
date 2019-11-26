#include "SEBBComparator.h"
#include "Compiler.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <list>
#include <memory>
#include <stack>
#include <vector>

using namespace llvm;
namespace ppa {

static const char* kPlaintiffExePath = "/tmp/ppa_detector_plaintiff";
static const char* kSuspiciousExePath = "/tmp/ppa_detector_suspicious";
static const char* kLogPath = "/tmp/ppa_detector_log";

constexpr uint32_t kBufferSize = 4 * 1024 * 1024;
constexpr uint64_t kLogDelimiter = 0xFFFFFFFFFFFFFFFF;
constexpr uint64_t kEnterBasicBlock = 0xFFFFFFFFFFFFFFFE;
constexpr uint64_t kExitBasicBlock = 0xFFFFFFFFFFFFFFFD;
constexpr uint64_t kInputMarker = 0x0000000000000000;
constexpr uint64_t kOutputMarker = 0x4000000000000000;

struct BBLog {
  std::vector<uint64_t> inputs;
  std::vector<uint64_t> outputs;
};

using RunLog = DenseMap<uint64_t, std::list<BBLog>>;

static RunLog readLogFromFile(uint64_t* buffer) {
  RunLog log;

  uint32_t pos = 0;
  std::stack<BBLog> stack;

  while (buffer[pos] != kLogDelimiter) {
    uint64_t op = buffer[pos++];
    uint64_t val = buffer[pos++];

    if (op == kEnterBasicBlock) {
      stack.emplace();
    } else if (op == kExitBasicBlock) {
      auto bbLog = stack.top();
      stack.pop();
      log[val].emplace_back(bbLog);
    } else if (op & kOutputMarker) {
      // uint64_t id = op & (~kOutputMarker);
      stack.top().outputs.emplace_back(val);
    } else {
      // uint64_t id = op & (~kInputMarker);
      stack.top().inputs.emplace_back(val);
    }
  }

  return log;
}

SEBBComparator::SEBBComparator(TestCaseLoader& loader) : loader_(loader) {}

void SEBBComparator::compareModules(Module& p, Module& s) {
  legacy::PassManager ppm, spm;
  ppm.add(new PlaintiffPass());
  ppm.add(createVerifierPass());
  ppm.run(p);

  spm.add(new SuspiciousPass());
  spm.add(createVerifierPass());
  spm.run(s);

  Compiler compiler;
  compiler.Compile(p, kPlaintiffExePath);
  compiler.Compile(s, kSuspiciousExePath);

  int fd = open(kLogPath, O_RDWR | O_CREAT, 0666);
  ftruncate(fd, kBufferSize);
  uint64_t* buffer = static_cast<uint64_t*>(
      mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

  int numTestCases = loader_.GetNumTestCases();

  for (int id = 0; id < numTestCases; id++) {
    StringRef testCasePath = loader_.GetTestCase(id);

    freopen(testCasePath.str().c_str(), "r", stdin);
    sys::ExecuteAndWait(kPlaintiffExePath, {kPlaintiffExePath});
    RunLog plaintiffLog = readLogFromFile(buffer);
    fclose(stdin);

    freopen(testCasePath.str().c_str(), "r", stdin);
    sys::ExecuteAndWait(kSuspiciousExePath, {kSuspiciousExePath});
    RunLog suspiciousLog = readLogFromFile(buffer);
    fclose(stdin);

    for (auto runLog : {plaintiffLog, suspiciousLog}) {
      for (auto bbLog : runLog) {
        outs() << "BB #" << bbLog.first << ", size: " << bbLog.second.size()
               << "\n";
        for (auto run : bbLog.second) {
          for (auto input : run.inputs) {
            outs() << "[I] " << input << "\n";
          }
          for (auto output : run.inputs) {
            outs() << "[O] " << output << "\n";
          }
          outs() << "\n";
        }
      }
      outs() << "\n\n\n";
    }
  }
}
} // namespace ppa