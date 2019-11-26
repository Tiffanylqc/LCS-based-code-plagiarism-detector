#ifndef PPADETECTOR_ALLFILESLOADER_H
#define PPADETECTOR_ALLFILESLOADER_H

#include <string>
#include <vector>

#include "TestCaseLoader.h"
#include "llvm/ADT/StringRef.h"

namespace ppa {

class AllFilesLoader : public TestCaseLoader {
public:
  void Initialize(llvm::StringRef folderPath) override;
  int GetNumTestCases() override;
  llvm::StringRef GetTestCase(int id) override;

private:
  std::vector<std::string> files;
};

} // namespace ppa

#endif