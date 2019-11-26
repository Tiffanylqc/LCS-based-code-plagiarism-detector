#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
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

static bool isValuedType(Type* ty) {
  // FIXME: Not sure if we should consider pointer types
  // and array types here
  return ty->isIntegerTy() || ty->isFloatingPointTy();
}

static void insertValuedIntoSet(DenseSet<Value*>& set, Value* val) {
  if (isValuedType(val->getType())) {
    set.insert(val);
  }
}

static DenseSet<Value*> computeValuedInputs(BasicBlock& bb) {
  DenseSet<Value*> inputs;

  for (auto& i : bb) {
    if (isa<AllocaInst>(i) || isa<PHINode>(i)) {
      // FIXME: Just an ad-hoc workaround...
      continue;
    }
    for (auto op : i.operand_values()) {
      if (isa<Instruction>(op) || isa<Constant>(op) || isa<Argument>(op)) {
        // FIXME: The last remaining type that op can be
        // is BasicBlock* according to my observation
        insertValuedIntoSet(inputs, op);
      }
    }
  }

  // Removes dependencies inside a basic block
  for (auto& i : bb) {
    inputs.erase(&i);
  }

  // Inserts the phi nodes back
  for (auto& i : bb) {
    if (isa<PHINode>(i)) {
      insertValuedIntoSet(inputs, &i);
    }
  }

  DenseSet<Value*> storeTo;

  for (auto& i : bb) {
    if (auto store = dyn_cast<StoreInst>(&i)) {
      storeTo.insert(store->getPointerOperand());
    }
  }

  for (auto& i : bb) {
    if (auto load = dyn_cast<LoadInst>(&i)) {
      if (storeTo.find(load->getPointerOperand()) == storeTo.end()) {
        insertValuedIntoSet(inputs, load);
      }
    }
  }

  return inputs;
}

static DenseSet<Value*> computeOutputs(BasicBlock& bb) {
  DenseSet<Value*> outputs;

  for (auto& i : bb) {
    if (std::any_of(i.user_begin(), i.user_end(), [&](auto user) {
          if (auto inst = dyn_cast<Instruction>(user)) {
            return inst->getParent() != &bb || inst == bb.getTerminator();
          }
          return false;
        })) {
      insertValuedIntoSet(outputs, &i);
    }
  }

  for (auto& i : bb) {
    if (auto store = dyn_cast<StoreInst>(&i)) {
      insertValuedIntoSet(outputs, store->getValueOperand());
    }
  }

  return outputs;
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

  auto* logTy = FunctionType::get(voidTy, {int64Ty, int64Ty}, false);
  auto logInputFun = m.getOrInsertFunction("SEBB_RUNTIME_logInput", logTy);
  auto logOutputFun = m.getOrInsertFunction("SEBB_RUNTIME_logOutput", logTy);

  appendToGlobalCtors(m, llvm::cast<Function>(initFun.getCallee()), 0);
  appendToGlobalDtors(m, llvm::cast<Function>(finalizeFun.getCallee()), 0);

  for (auto& f : m) {
    for (auto& bb : f) {
      uint64_t ID = idMap[&bb];

      // FIXME: This is probably not exhaustive
      IRBuilder<> builder(bb.getTerminator());

      auto* IDVal = builder.getInt64(ID);
      for (auto* val : computeValuedInputs(bb)) {
        auto zext = builder.CreateZExt(val, int64Ty);
        builder.CreateCall(logInputFun, {IDVal, zext});
      }
      for (auto* val : computeOutputs(bb)) {
        auto zext = builder.CreateZExt(val, int64Ty);
        builder.CreateCall(logOutputFun, {IDVal, zext});
      }
      builder.CreateCall(exitFun.getCallee(), {IDVal});

      if (bb.getFirstInsertionPt() != bb.end()) {
        builder.SetInsertPoint(&*bb.getFirstInsertionPt());
      } else {
        // FIXME: What if bb is empty?
        errs() << "Basic block is empty.\n";
      }
      builder.CreateCall(enterFun.getCallee(), IDVal);
    }
  }

  return true;
}