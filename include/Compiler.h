#ifndef PPADETECTOR_COMPILER_H
#define PPADETECTOR_COMPILER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"

namespace ppa {

class Compiler {
public:
  Compiler(); 
  void Compile(llvm::Module& module, llvm::StringRef outFile);
};

} // namespace ppa

#endif