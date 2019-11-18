#include "InstHistComparator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>
#include <numeric>

using namespace llvm;
namespace ppa {

static void insertIntoHistogram(InstHistogram& histogram, unsigned int opcode,
                                double count = 1.0) {
  auto iter = histogram.find(opcode);
  if (iter == histogram.end()) {
    iter = histogram.insert(std::make_pair(opcode, 0)).first;
  }
  iter->second += count;
}

static void normalizeHistogram(InstHistogram& histogram) {
  auto sum = std::accumulate(
      histogram.begin(), histogram.end(), 0.0,
      [](const auto& acc, const auto& iter) { return acc + iter.second; });
  for (auto& [opcode, count] : histogram) {
    count /= sum;
  }
}

static double computeChiSquareDistance(InstHistogram& p, InstHistogram& s) {
  normalizeHistogram(p);
  normalizeHistogram(s);
  InstHistogram merged;
  for (const auto& [opcode, count] : p) {
    insertIntoHistogram(merged, opcode, count);
  }
  for (const auto& [opcode, count] : s) {
    insertIntoHistogram(merged, opcode, count);
  }
  double result = 0.0;
  for (const auto& [opcode, count] : merged) {
    double diff = p.lookup(opcode) - s.lookup(opcode);
    result += diff * diff / count;
  }
  return result / 2;
}

char InstHistPass::ID = 0;

bool InstHistPass::runOnModule(Module& m) {
  for (auto& f : m) {
    for (auto& bb : f) {
      for (auto& i : bb) {
        handleInstruction(&i);
      }
    }
  }
  return false;
}

void InstHistPass::handleInstruction(llvm::Instruction* i) {
  auto opcode = i->getOpcode();
  insertIntoHistogram(*histogram, opcode);
}

void InstHistComparator::compareModules(Module& p, Module& s) {
  InstHistogram pHist, sHist;
  legacy::PassManager ppm, spm;
  ppm.add(new PlaintiffPass(&pHist));
  spm.add(new SuspiciousPass(&sHist));
  ppm.run(p);
  spm.run(s);
  double score = 1.0 - computeChiSquareDistance(pHist, sHist);
  outs() << (int)(std::round(score * 100)) << "%\n";
}

} // namespace ppa