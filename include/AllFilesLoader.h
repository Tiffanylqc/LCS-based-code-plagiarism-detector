#ifndef PPADETECTOR_ALLFILESLOADER_H
#define PPADETECTOR_ALLFILESLOADER_H

#include "llvm/ADT/StringRef.h"
#include "TestCaseLoader.h"

namespace ppa {

class AllFilesLoader : public TestCaseLoader {
public:
  void Initialize(llvm::StringRef folderPath) override;
  int GetNumTestCases() override;
  llvm::StringRef GetTestCase(int id) override;
};

} // namespace ppa

#endif