#ifndef PPADETECTOR_SEBBCOMPARATOR_H
#define PPADETECTOR_SEBBCOMPARATOR_H

#include "BBLoggingPass.h"
#include "Comparator.h"

namespace ppa {

class SEBBComparator : public Comparator<BBLoggingPass, BBLoggingPass> {
public:
  void compareModules(llvm::Module& p, llvm::Module& s) override;
  ~SEBBComparator() = default;
};

} // namespace ppa

#endif