#ifndef PPADETECTOR_IDENTITYCOMPARATOR_H
#define PPADETECTOR_IDENTITYCOMPARATOR_H

#include "Comparator.h"
#include "llvm/ADT/DenseMap.h"

namespace ppa {

using InstHistogram = llvm::DenseMap<unsigned int, double>;

struct InstHistPass : public llvm::ModulePass {
  static char ID;
  InstHistogram* histogram;
  InstHistPass(InstHistogram* histogram)
      : llvm::ModulePass(ID), histogram(histogram) {}
  bool runOnModule(llvm::Module& m) override;
  void handleInstruction(llvm::Instruction* i);
};

class InstHistComparator : public Comparator<InstHistPass, InstHistPass> {
public:
  void compareModules(llvm::Module& p, llvm::Module& s) override;
  ~InstHistComparator() = default;
};

} // namespace ppa

#endif