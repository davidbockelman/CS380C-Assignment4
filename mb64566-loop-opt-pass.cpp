#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"  
#include "llvm/Analysis/ValueTracking.h"

using namespace llvm;

namespace {

bool isSafeToHoist(Instruction &I, Loop *L, DominatorTree &DT) {
  if (!isSafeToSpeculativelyExecute(&I)) return false;

  SmallVector<BasicBlock *, 8> exitBlocks;
  L->getExitBlocks(exitBlocks);
  for (BasicBlock *ExitBB : exitBlocks) {
      if (!DT.dominates(I.getParent(), ExitBB)) return false;
  }

  return true;
}


bool isLoopInvariant(Instruction &I, Loop *L) {
  if (!I.isBinaryOp() && !isa<SelectInst>(&I) && !isa<CastInst>(&I) && 
      !isa<GetElementPtrInst>(I)) {
      return false;
  }

  for (Value *Op : I.operands()) {
      if (!isa<Constant>(Op) && L->contains(dyn_cast<Instruction>(Op))) {
          return false;
      }
  }
  return true;
}


void hoistLoopInvariants(Loop *L, DominatorTree &DT) {
  BasicBlock *Preheader = L->getLoopPreheader();
  if (!Preheader) return;

  SmallVector<Instruction *, 8> ToHoist;

  for (BasicBlock *BB : L->blocks()) {
      if (L->getHeader() == BB) continue;

      for (Instruction &I : *BB) {
          if (isLoopInvariant(I, L) && isSafeToHoist(I, L, DT)) {
              errs() << "Hoistable: " << I << "\n";
              ToHoist.push_back(&I);
          }
      }
  }

  for (Instruction *I : ToHoist) {
      I->moveBefore(Preheader->getTerminator());
      errs() << "Hoisted: " << *I << "\n";
  }
}


// New PM implementation
struct LoopPass : PassInfoMixin<LoopPass> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    errs() << "Running LICM on function: " << F.getName() << "\n";

    // Get loop information and dominator tree
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    for (Loop *L : LI) {
        if (!L->getLoopPreheader()) {
            errs() << "Skipping loop without preheader\n";
            continue;
        }
        hoistLoopInvariants(L, DT);
    }

    return PreservedAnalyses::all();
  }

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mb64566-Loop-Opt-Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mb64566-loop-opt-pass") {
		    FPM.addPass(LoopSimplifyPass());
                    FPM.addPass(LoopPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}
