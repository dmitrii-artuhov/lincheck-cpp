#include "llvm/Pass.h"

#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "utils.h"

// Function names for which we generate coroutines.
std::vector<std::string> gen_coro_list = {
    "myFunc",
};

using builder_t = IRBuilder<NoFolder>;

void rawGenCoroFunc(Module &M, Function &F) {
  auto &ctx = M.getContext();
  auto ptr = PointerType::get(ctx, 0);
  auto i1 = Type::getInt1Ty(ctx);
  auto i8 = Type::getInt8Ty(ctx);
  auto i32 = Type::getInt32Ty(ctx);
  auto token = Type::getTokenTy(ctx);
  auto i32_0 = ConstantInt::get(i32, 0);
  auto ptr_null = ConstantPointerNull::get(ptr);
  auto token_none = ConstantTokenNone::get(ctx);
  auto i1_false = ConstantInt::get(i1, false);

  assert(ptr == F.getReturnType() && "F must have ptr return type");
  F.setPresplitCoroutine();

  auto &first_real_block = F.front();
  auto suspend = BasicBlock::Create(ctx, "suspend", &F, &F.front());
  auto cleanup = BasicBlock::Create(ctx, "cleanup", &F, suspend);
  auto init = BasicBlock::Create(ctx, "init", &F, cleanup);

  builder_t Builder(init);

  // init:
  Builder.SetInsertPoint(init);
  auto id = Builder.CreateIntrinsic(token, Intrinsic::coro_id,
                                    {i32_0, ptr_null, ptr_null, ptr_null},
                                    nullptr, "id");
  auto size =
      Builder.CreateIntrinsic(i32, Intrinsic::coro_size, {}, nullptr, "size");
  auto alloc =
      Builder.CreateCall(utils::GenMallocCallee(M), {size}, "hdl_alloc");
  auto hdl = Builder.CreateIntrinsic(ptr, Intrinsic::coro_begin, {id, alloc},
                                     nullptr, "hdl");
  auto suspend_0 = Builder.CreateIntrinsic(i8, Intrinsic::coro_suspend,
                                           {token_none, i1_false});
  auto _switch = Builder.CreateSwitch(suspend_0, suspend, 2);
  _switch->addCase(ConstantInt::get(i8, 0), &first_real_block);
  _switch->addCase(ConstantInt::get(i8, 1), cleanup);

  // cleanup:
  Builder.SetInsertPoint(cleanup);
  auto mem = Builder.CreateIntrinsic(ptr, Intrinsic::coro_free, {id, hdl});
  Builder.CreateCall(utils::GenFreeCallee(M), {mem});
  Builder.CreateBr(suspend);

  // suspend:
  Builder.SetInsertPoint(suspend);
  auto unused = Builder.CreateIntrinsic(i1, Intrinsic::coro_end,
                                        {hdl, i1_false, token_none});
  Builder.CreateRet(hdl);

  // Change terminating instructions.
  for (int i = 0; auto &b : F) {
    ++i;
    if (i < 4) {
      // We must skip [init, cleanup, suspend].
      continue;
    }
    auto &terminate_instr = b.back();
    if (dyn_cast<ReturnInst>(&terminate_instr)) {
      ReplaceInstWithInst(&terminate_instr, BranchInst::Create(cleanup));
    }
  }
}

void GenCoroFunc(Module &M, Function &F) {
  assert(!F.empty() && "function must not be empty");
  auto ptr = PointerType::get(M.getContext(), 0);
  auto newF = utils::CloneFuncChangeRetType(&F, ptr, F.getName() + "_coro");
  rawGenCoroFunc(M, *newF);
}

namespace {

struct CoroGenPass : public PassInfoMixin<CoroGenPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      auto name = F.getName();
      if (std::find(gen_coro_list.begin(), gen_coro_list.end(), name) !=
          gen_coro_list.end()) {
        errs() << "Gen coro clone for " << name << "\n";
        GenCoroFunc(M, F);
      }
    }
    return PreservedAnalyses::none();
  };
};

}  // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "coro_gen",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coro_gen") {
                    MPM.addPass(CoroGenPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
