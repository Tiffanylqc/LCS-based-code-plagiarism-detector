#include "SEBBComparator.h"
#include "Compiler.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace ppa {

static const char* plaintiffExePath = "/tmp/ppa_detector_plaintiff";
static const char* suspiciousExePath = "/tmp/ppa_detector_suspicious";

void SEBBComparator::compareModules(Module& p, Module& s) {
  legacy::PassManager ppm, spm;
  ppm.add(new PlaintiffPass());
  ppm.add(createVerifierPass());
  ppm.run(p);
  spm.add(new SuspiciousPass());
  spm.add(createVerifierPass());
  spm.run(s);

  Compiler compiler;
  compiler.Compile(p, plaintiffExePath);
  compiler.Compile(s, suspiciousExePath);

  sys::ExecuteAndWait(plaintiffExePath, {plaintiffExePath});
  sys::ExecuteAndWait(suspiciousExePath, {suspiciousExePath});
}
} // namespace ppa