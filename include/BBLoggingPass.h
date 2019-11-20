#ifndef PPADETECTOR_BBLOGGINGPASS_H
#define PPADETECTOR_BBLOGGINGPASS_H

#include "llvm/Pass.h"

namespace ppa {

struct BBLoggingPass : public llvm::ModulePass {
  static char ID;

  BBLoggingPass() : llvm::ModulePass(ID) {}

  bool runOnModule(llvm::Module& m) override;
};

} // namespace ppa

#endif