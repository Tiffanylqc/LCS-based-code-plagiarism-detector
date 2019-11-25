#ifndef PPADETECTOR_TESTCASELOADER_H
#define PPADETECTOR_TESTCASELOADER_H

#include "llvm/ADT/StringRef.h"

namespace ppa {

class TestCaseLoader {
public:
  // Opens a folder with test cases
  virtual void Initialize(llvm::StringRef folderPath) = 0;
  virtual int GetNumTestCases() = 0;
  virtual llvm::StringRef GetTestCase(int id) = 0;
};

} // namespace ppa

#endif