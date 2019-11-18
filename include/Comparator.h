#ifndef PPADETECTOR_COMPARATOR_H
#define PPADETECTOR_COMPARATOR_H

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <type_traits>

namespace ppa {

template <typename P, typename S,
          typename =
              std::enable_if_t<std::is_base_of<llvm::ModulePass, P>::value &&
                               std::is_base_of<llvm::ModulePass, S>::value>>
class Comparator {
public:
  using PlaintiffPass = P;
  using SuspiciousPass = S;
  virtual void compareModules(llvm::Module& p, llvm::Module& s) = 0;
  virtual ~Comparator() = default;
};
} // namespace ppa

#endif