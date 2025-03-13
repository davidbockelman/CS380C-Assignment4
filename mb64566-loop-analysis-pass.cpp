#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

int loop_count = 0;

bool is_top_level(Loop *L, BasicBlock *BB) {
  for (Loop *sub_loop : L->getSubLoops()) {
      if (sub_loop->contains(BB)) {
          return false;
      }
  }

  return true;
}

void get_loop_info(Loop *L, int &num_bbs, int &num_insns, int &num_atoms, int &num_branches) {
  num_bbs = 0;
  num_insns = 0;
  num_atoms = 0;
  num_branches = 0;
  for (BasicBlock *BB : L->blocks()) {
      if (is_top_level(L, BB)) {
          num_bbs++;
          if (isa<BranchInst>(BB->getTerminator())) {
            errs() << "BranchInst: " << BB->getName() << "\n";
              num_branches++;
          }
      }
      num_insns += BB->size();
      for (Instruction &I : *BB) {
        errs() << I << " Opcode: " << I.getOpcode() << "\n";
          if (isa<AtomicRMWInst>(&I) || isa<AtomicCmpXchgInst>(&I)) {
              num_atoms++;
          }
          // if (isa<BranchInst>(&I)) {
          //     num_branches++;
          // }
        // std::string instrStr;
        // llvm::raw_string_ostream rso(instrStr);
        // I.print(rso);
        // rso.flush();

        // if (instrStr.rfind("br", 0) == 0) { // Check if it starts with "br"
        //     llvm::errs() << "Branch Instruction Found: " << instrStr << "\n";
        // }
      }
  }
}

void process_loop(Loop *L, Function &F) {
  int num_bbs;
  int num_insns;
  int num_atoms;
  int num_branches;
  get_loop_info(L, num_bbs, num_insns, num_atoms, num_branches);
  errs() << "<" << loop_count++ << ">: ";
  errs() << "func=<" << F.getName() << ">, ";
  errs() << "depth=<" << (L->getLoopDepth() - 1) << ">, ";
  errs() << "subLoops=<" << ((L->getSubLoops().size() > 0) ? "true" : "false") << ">, ";
  errs() << "BBs=<" << num_bbs << ">, ";
  errs() << "instrs=<" << num_insns << ">, ";
  errs() << "atomics=<" << num_atoms << ">, ";
  errs() << "branches=<" << num_branches << ">\n";

  for (Loop *sub_loop : L->getSubLoops()) {
      process_loop(sub_loop, F);
  }
}


// New PM implementation
struct LoopPass : PassInfoMixin<LoopPass> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

    auto &LI = FAM.getResult<LoopAnalysis>(F);

    for (Loop *L : LI) {
        process_loop(L, F);
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
  return {LLVM_PLUGIN_API_VERSION, "mb64566-Loop-Analysis-Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mb64566-loop-analysis-pass") {
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
