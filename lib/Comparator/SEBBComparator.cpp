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

using ControlFlowTraceLog = std::vector<uint64_t>;
static ControlFlowTraceLog readCFTLogFromFile(uint64_t* buffer) {
  ControlFlowTraceLog log;

  uint32_t pos = 0;
  std::stack<BBLog> stack;

  while (buffer[pos] != kLogDelimiter) {
    uint64_t op = buffer[pos++];
    uint64_t val = buffer[pos++];

    if (op == kEnterBasicBlock) {
      // the dynamic CFG almost forms a tree, and
      // we only report the post-order traversal
    } else if (op == kExitBasicBlock) {
      log.emplace_back(val);
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

template <class T>
static int computeLCS(std::vector<uint64_t>& p, std::vector<uint64_t>& s,
                      T cmp) {
  std::vector<int> dp_data[2];
  dp_data[0].resize(p.size() + 1);
  dp_data[1].resize(p.size() + 1);

  auto dp = [&](int i, int j) -> int& { return dp_data[i % 2][j]; };

  for (size_t i = 1; i <= s.size(); i++) {
    for (size_t j = 1; j <= p.size(); j++) {
      if (cmp(p[j - 1], s[i - 1])) {
        dp(i, j) = dp(i - 1, j - 1) + 1;
      } else {
        dp(i, j) = std::max(dp(i - 1, j), dp(i, j - 1));
      }
    }
  }

  return dp(s.size(), p.size());
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
      } else if (pLog.inputs.size() == 0 || sLog.inputs.size() == 0) {
        iratio = 0;
      } else {
        iratio = (double)icnt / pLog.inputs.size();
      }

      int ocnt = computeIntersection(pLog.outputs, sLog.outputs);
      double oratio = 0;
      if (pLog.outputs.size() == 0 && sLog.outputs.size() == 0) {
        oratio = 1;
      } else if (pLog.outputs.size() == 0 || sLog.outputs.size() == 0) {
        oratio = 0;
      } else {
        oratio = (double)ocnt / pLog.outputs.size();
      }
      if (iratio >= kInputRatioCutoff && oratio >= kOutputRatioCutoff) {
        similar++;
        break;
      }
    }
  }
  for (auto& sLog : sLogs) {
    for (auto& pLog : pLogs) {
      int icnt = computeIntersection(pLog.inputs, sLog.inputs);
      double iratio = 0;
      if (pLog.inputs.size() == 0 && sLog.inputs.size() == 0) {
        iratio = 1;
      } else if (pLog.inputs.size() == 0 || sLog.inputs.size() == 0) {
        iratio = 0;
      } else {
        iratio = (double)icnt / sLog.inputs.size();
      }

      int ocnt = computeIntersection(pLog.outputs, sLog.outputs);
      double oratio = 0;
      if (pLog.outputs.size() == 0 && sLog.outputs.size() == 0) {
        oratio = 1;
      } else if (pLog.outputs.size() == 0 || sLog.outputs.size() == 0) {
        oratio = 0;
      } else {
        oratio = (double)ocnt / sLog.outputs.size();
      }
      if (iratio >= kInputRatioCutoff && oratio >= kOutputRatioCutoff) {
        similar++;
        break;
      }
    }
  }
  double ratio = (double)similar / (pLogs.size() + sLogs.size());
  return (ratio >= kBBSimilarityCutoff);
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

  DenseMap<uint64_t, DenseMap<uint64_t, double>> SEBB;

  for (int id = 0; id < numTestCases - 1; id++) {
    StringRef testCasePath = loader_.GetTestCase(id);

    sys::ExecuteAndWait(kPlaintiffExePath, {kPlaintiffExePath}, None,
                        {testCasePath, StringRef(), StringRef()});
    RunLog plaintiffLog = readLogFromFile(buffer);

    sys::ExecuteAndWait(kSuspiciousExePath, {kSuspiciousExePath}, None,
                        {testCasePath, StringRef(), StringRef()});
    RunLog suspiciousLog = readLogFromFile(buffer);

    for (uint64_t p = 1; p <= plaintiffLog.size(); ++p) {
      for (uint64_t s = 1; s <= suspiciousLog.size(); ++s) {
        // if (s != p) continue;
        auto& pLogs = plaintiffLog[p];
        auto& sLogs = suspiciousLog[s];
        auto t = compareBBSimilarity(pLogs, sLogs);
        SEBB[p][s] += t;
      }
    }
  }

  const double simThreshold = 0.8 * (numTestCases - 1);

  for (int id = numTestCases - 1; id < numTestCases; id++) {
    StringRef testCasePath = loader_.GetTestCase(id);

    sys::ExecuteAndWait(kPlaintiffExePath, {kPlaintiffExePath}, None,
                        {testCasePath, StringRef(), StringRef()});
    ControlFlowTraceLog plaintiffLog = readCFTLogFromFile(buffer);

    sys::ExecuteAndWait(kSuspiciousExePath, {kSuspiciousExePath}, None,
                        {testCasePath, StringRef(), StringRef()});
    ControlFlowTraceLog suspiciousLog = readCFTLogFromFile(buffer);

    outs() << "pSize: " << plaintiffLog.size() << "\n";
    outs() << "sSize: " << suspiciousLog.size() << "\n";
    outs() << "LCS:   "
           << computeLCS(plaintiffLog, suspiciousLog,
                         [&](uint64_t p, uint64_t s) {
                           return SEBB[p][s] >= simThreshold;
                         });
  }
}
} // namespace ppa