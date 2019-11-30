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

#include <algorithm>
#include <cmath>
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

constexpr double kInputRatioCutoff = 1.0;
constexpr double kOutputRatioCutoff = 1.0;
constexpr double kBBSimilarityCutoff = 1.0;

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

static int computeIntersection(std::vector<uint64_t>& p,
                               std::vector<uint64_t>& s) {
  int cnt = 0;

  std::vector<uint64_t> sortedp(p), sorteds(s);
  std::sort(sortedp.begin(), sortedp.end());
  std::sort(sorteds.begin(), sorteds.end());
  auto firstp = sortedp.begin(), lastp = sortedp.end();
  auto firsts = sorteds.begin(), lasts = sorteds.end();

  while (firstp != lastp && firsts != lasts) {
    if (*firstp < *firsts) {
      ++firstp;
    } else {
      if (*firsts == *firstp) {
        ++cnt;
        ++firstp;
      }
      ++firsts;
    }
  }

  return cnt;
}

static double compareBBSimilarity(std::list<BBLog>& pLogs,
                                  std::list<BBLog>& sLogs) {
  int similar = 0;
  for (auto& pLog : pLogs) {
    for (auto& sLog : sLogs) {
      int icnt = computeIntersection(pLog.inputs, sLog.inputs);

      double iratio = 0;
      if (pLog.inputs.size() == 0 && sLog.inputs.size() == 0) {
        iratio = 1;
      } else if (pLog.inputs.size() == 0 && sLog.inputs.size() != 0) {
        iratio = 0;
      } else {
        iratio = (double)icnt / pLog.inputs.size();
      }

      // if (iratio <= kInputRatioCutoff)
      //  continue;
      int ocnt = computeIntersection(pLog.outputs, sLog.outputs);
      double oratio = 0;
      if (pLog.outputs.size() == 0 && sLog.outputs.size() == 0) {
        oratio = 1;
      } else if (pLog.outputs.size() == 0 && sLog.outputs.size() != 0) {
        oratio = 0;
      } else {
        oratio = (double)ocnt / pLog.outputs.size();
      }

      // if (oratio <= kOutputRatioCutoff)
      //  continue;
      // outs() << "i%:" << icnt << "/" << pLog.inputs.size() << "\n";
      // outs() << "o%:" << ocnt << "/" << pLog.outputs.size() << "\n";
      if (iratio >= kInputRatioCutoff && oratio >= kOutputRatioCutoff)
        similar++;
    }
  }
  // outs() << similar << "/(" << pLogs.size() << "," << sLogs.size() << ")\n";
  double ratio = (double)similar / pLogs.size();
  return (ratio > kBBSimilarityCutoff);
}

SEBBComparator::SEBBComparator(TestCaseLoader& loader) : loader_(loader) {}

void SEBBComparator::compareModules(Module& p, Module& s) {
  DenseMap<uint64_t, BasicBlock*> pBBMap, sBBMap;

  legacy::PassManager ppm, spm;
  ppm.add(new PlaintiffPass(pBBMap));
  ppm.add(createVerifierPass());
  ppm.run(p);

  spm.add(new SuspiciousPass(sBBMap));
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
  
    for (uint64_t p = 1; p <= plaintiffLog.size(); ++p) {
      for (uint64_t s = 1; s <= suspiciousLog.size(); ++s) {
        assert(plaintiffLog.count(p) == 1);
        assert(suspiciousLog.count(s) == 1);
        // if (s != p) continue;
        auto& pLogs = plaintiffLog[p];
        auto& sLogs = suspiciousLog[s];
        auto t = compareBBSimilarity(pLogs, sLogs);
        if (t) {
          if (s == p)
            outs() << "[X]";
          else
            outs() << " X ";
        } else {
          if (s == p)
            outs() << "[.]";
          else
            outs() << " . ";
        }
      }
      outs() << "\n";
    }
  }
}
} // namespace ppa