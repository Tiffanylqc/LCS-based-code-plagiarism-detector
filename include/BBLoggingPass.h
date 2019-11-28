#ifndef PPADETECTOR_BBLOGGINGPASS_H
#define PPADETECTOR_BBLOGGINGPASS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Pass.h"

namespace ppa {

struct BBLoggingPass : public llvm::ModulePass {
  static char ID;

  BBLoggingPass(llvm::DenseMap<uint64_t, llvm::BasicBlock*>& idMap)
      : llvm::ModulePass(ID), idMap_(idMap) {}

  bool runOnModule(llvm::Module& m) override;

  llvm::DenseMap<uint64_t, llvm::BasicBlock*>& idMap_;
};

} // namespace ppa

#endif