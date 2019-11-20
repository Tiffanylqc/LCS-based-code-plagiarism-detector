#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "InstHistComparator.h"
#include "SEBBComparator.h"

#include <memory>

using namespace llvm;

enum class AnalysisType { InstHist, SEBB };

static cl::OptionCategory ppaDetectorCategory{"ppa-detector options"};

static cl::opt<std::string> plaintiffPath{cl::Positional,
                                          cl::desc{"<plaintiff>"},
                                          cl::value_desc{"bitcode filename"},
                                          cl::init(""),
                                          cl::Required,
                                          cl::cat{ppaDetectorCategory}};

static cl::opt<std::string> suspiciousPath{cl::Positional,
                                           cl::desc{"<suspicious>"},
                                           cl::value_desc{"bitcode filename"},
                                           cl::init(""),
                                           cl::Required,
                                           cl::cat{ppaDetectorCategory}};

static cl::opt<AnalysisType> analysisType{
    cl::desc{"Analysis type:"},
    cl::values(
        clEnumValN(AnalysisType::InstHist, "instruction-histogram",
                   "Measure similarity based on instruction histograms"),
        clEnumValN(AnalysisType::SEBB, "sebb",
                   "Measure similarity based on semantically equivalent basic "
                   "blocks")),
    cl::Required, cl::cat{ppaDetectorCategory}};

cl::list<std::string> libPaths{
    "L", cl::Prefix, cl::desc{"Specify a library search path"},
    cl::value_desc{"directory"}, cl::cat{ppaDetectorCategory}};

cl::list<std::string> libraries{
    "l", cl::Prefix, cl::desc{"Specify libraries to link against"},
    cl::value_desc{"library prefix"}, cl::cat{ppaDetectorCategory}};

static void compareInstHist(Module& p, Module& s) {
  auto comparator = std::make_unique<ppa::InstHistComparator>();
  comparator->compareModules(p, s);
}

static void compareSEBB(Module& p, Module& s) {
  auto comparator = std::make_unique<ppa::SEBBComparator>();
  comparator->compareModules(p, s);
}

int main(int argc, char** argv) {
  // This boilerplate provides convenient stack traces and clean LLVM exit
  // handling. It also initializes the built in support for convenient
  // command line option handling.
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj shutdown;
  cl::HideUnrelatedOptions(ppaDetectorCategory);
  cl::ParseCommandLineOptions(argc, argv);

  // Construct an IR file from the filename passed on the command line.
  SMDiagnostic err_p;
  LLVMContext context_p;
  std::unique_ptr<Module> plaintiffModule =
      parseIRFile(plaintiffPath.getValue(), err_p, context_p);

  if (!plaintiffModule.get()) {
    errs() << "Error reading bitcode file: " << plaintiffPath << "\n";
    err_p.print(argv[0], errs());
    return -1;
  }

  SMDiagnostic err_s;
  LLVMContext context_s;
  std::unique_ptr<Module> suspiciousModule =
      parseIRFile(suspiciousPath.getValue(), err_s, context_s);

  if (!suspiciousModule.get()) {
    errs() << "Error reading bitcode file: " << suspiciousPath << "\n";
    err_s.print(argv[0], errs());
    return -1;
  }

  if (analysisType == AnalysisType::InstHist) {
    compareInstHist(*plaintiffModule, *suspiciousModule);
  } else if (analysisType == AnalysisType::SEBB) {
    compareSEBB(*plaintiffModule, *suspiciousModule);
  }

  return 0;
}