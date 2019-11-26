#ifndef PPADETECTOR_SEBBCOMPARATOR_H
#define PPADETECTOR_SEBBCOMPARATOR_H

#include "BBLoggingPass.h"
#include "Comparator.h"
#include "TestCaseLoader.h"

namespace ppa {

class SEBBComparator : public Comparator<BBLoggingPass, BBLoggingPass> {
public:
  SEBBComparator(TestCaseLoader& loader);
  void compareModules(llvm::Module& p, llvm::Module& s) override;
  ~SEBBComparator() = default;

private:
  TestCaseLoader& loader_;
};

} // namespace ppa

#endif