#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "BBLoggingPass.h"

using namespace llvm;

namespace ppa {

char BBLoggingPass::ID = 0;

RegisterPass<BBLoggingPass> X("BBLoggingPass",
                              "Log the inputs and outputs of basic blocks");

} // namespace ppa

static DenseMap<BasicBlock*, uint64_t>
computeBasicBlockIDs(ArrayRef<BasicBlock*> basicBlocks) {
  DenseMap<BasicBlock*, uint64_t> idMap;

  uint64_t nextID = 1;
  for (auto bb : basicBlocks) {
    if (idMap.count(bb)) {
      continue;
    }
    idMap[bb] = nextID++;
  }

  return idMap;
}

bool ppa::BBLoggingPass::runOnModule(Module& m) {
  auto& context = m.getContext();

  std::vector<BasicBlock*> basicBlocks;
  for (auto& f : m) {
    for (auto& bb : f) {
      basicBlocks.push_back(&bb);
    }
  }

  auto idMap = computeBasicBlockIDs(basicBlocks);

  auto* voidTy = Type::getVoidTy(context);
  auto* int64Ty = Type::getInt64Ty(context);

  auto* helperTy = FunctionType::get(voidTy, int64Ty, false);
  auto enterFun = m.getOrInsertFunction("SEBB_RUNTIME_enter", helperTy);
  auto exitFun = m.getOrInsertFunction("SEBB_RUNTIME_exit", helperTy);

  auto initFun = m.getOrInsertFunction("SEBB_RUNTIME_init", voidTy);
  auto finalizeFun = m.getOrInsertFunction("SEBB_RUNTIME_finalize", voidTy);

  appendToGlobalCtors(m, llvm::cast<Function>(initFun.getCallee()), 0);
  appendToGlobalDtors(m, llvm::cast<Function>(finalizeFun.getCallee()), 0);

  for (auto& f : m) {
    for (auto& bb : f) {
      uint64_t ID = idMap[&bb];
      IRBuilder<> builder(&bb);
      // FIXME(shiges): This is probably not exhaustive
      if (auto* ret = dyn_cast<ReturnInst>(bb.getTerminator())) {
        builder.SetInsertPoint(ret);
      } else if (auto* br = dyn_cast<BranchInst>(bb.getTerminator())) {
        builder.SetInsertPoint(br);
      }
      auto* IDVal = builder.getInt64(ID);
      builder.CreateCall(exitFun.getCallee(), {IDVal});

      if (bb.getFirstInsertionPt() != bb.end()) {
        builder.SetInsertPoint(&*bb.getFirstInsertionPt());
      } else {
        // FIXME(shiges): What if bb is empty?
        errs() << "Basic block is empty.\n";
      }
      builder.CreateCall(enterFun.getCallee(), IDVal);
    }
  }

  return true;
}