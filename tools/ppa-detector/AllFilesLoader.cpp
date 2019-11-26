#include "AllFilesLoader.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace ppa {

void AllFilesLoader::Initialize(llvm::StringRef folderPath) {
  for (const auto& entry : fs::directory_iterator(folderPath.str())) {
    files.push_back(entry.path().string());
  }
}

int AllFilesLoader::GetNumTestCases() { return files.size(); }

llvm::StringRef AllFilesLoader::GetTestCase(int id) {
  return llvm::StringRef(files[id]);
}

} // namespace ppa