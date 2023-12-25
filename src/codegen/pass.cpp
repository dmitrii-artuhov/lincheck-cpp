#include "llvm/Pass.h"

#include <tuple>

#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "utils.h"

// Function names for which we generate coroutines.
std::vector<std::string> gen_coro_list = {
    "foo",
    "bar",
    "mini",
};

bool IsCoroTarget(const std::string &target) {
  return std::find(gen_coro_list.begin(), gen_coro_list.end(), target) !=
         gen_coro_list.end();
}

Twine ToCoro(const Twine &name) { return name + "_coro"; }

struct CoroGenerator final {
  CoroGenerator(Module &M) : M(M) {
    auto &ctx = M.getContext();
    i1_t = Type::getInt1Ty(ctx);
    i8_t = Type::getInt8Ty(ctx);
    i32_t = Type::getInt32Ty(ctx);
    ptr_t = PointerType::get(ctx, 0);
    void_t = Type::getVoidTy(ctx);
    token_t = Type::getTokenTy(ctx);
    promise_t = StructType::create("CoroPromise", i32_t, i32_t, ptr_t);
    promise_ptr_t = PointerType::get(promise_t, 0);

    // Names clashes?
    set_child_hdl = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, ptr_t}, false),
        Function::ExternalLinkage, "set_child_hdl", M);
    set_ret_val = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, i32_t}, false),
        Function::ExternalLinkage, "set_ret_val", M);
    get_ret_val =
        Function::Create(FunctionType::get(i32_t, {promise_ptr_t}, false),
                         Function::ExternalLinkage, "get_ret_val", M);
  }

  void Run() {
    for (auto &F : M) {
      auto name = F.getName().str();
      if (IsCoroTarget(name)) {
        GenCoroFunc(&F);
      }
    }
  }

 private:
  using builder_t = IRBuilder<NoFolder>;
  Module &M;
  Type *void_t;
  Type *token_t;
  StructType *promise_t;
  IntegerType *i1_t;
  IntegerType *i8_t;
  IntegerType *i32_t;
  PointerType *ptr_t;
  PointerType *promise_ptr_t;
  Function *set_child_hdl;
  Function *set_ret_val;
  Function *get_ret_val;

  Function *GenCoroFunc(Function *F) {
    assert(!F->empty() && "function must not be empty");
    auto coro_name = ToCoro(F->getName());
    SmallVector<char, 10> vec;
    if (auto func = M.getFunction(coro_name.toStringRef(vec))) {
      // Was generated later.
      // TODO: what if this symbol is defined in user code?
      return func;
    }
    errs() << "Gen " << coro_name << "\n";
    auto newF = utils::CloneFuncChangeRetType(F, ptr_t, coro_name);
    return rawGenCoroFunc(newF);
  }

  Function *rawGenCoroFunc(Function *F) {
    auto &ctx = M.getContext();

    auto i32_0 = ConstantInt::get(i32_t, 0);
    auto ptr_null = ConstantPointerNull::get(ptr_t);
    auto token_none = ConstantTokenNone::get(ctx);
    auto i1_false = ConstantInt::get(i1_t, false);

    assert(ptr_t == F->getReturnType() && "F must have ptr return type");
    F->setPresplitCoroutine();

    auto &first_real_block = F->front();
    auto suspend = BasicBlock::Create(ctx, "suspend", F, &F->front());
    auto cleanup = BasicBlock::Create(ctx, "cleanup", F, suspend);
    auto init = BasicBlock::Create(ctx, "init", F, cleanup);

    builder_t Builder(init);

    // init:
    Builder.SetInsertPoint(init);
    auto promise = Builder.CreateAlloca(promise_t, nullptr, "promise");
    auto id = Builder.CreateIntrinsic(token_t, Intrinsic::coro_id,
                                      {i32_0, promise, ptr_null, ptr_null},
                                      nullptr, "id");
    auto size = Builder.CreateIntrinsic(i32_t, Intrinsic::coro_size, {},
                                        nullptr, "size");
    auto alloc =
        Builder.CreateCall(utils::GenMallocCallee(M), {size}, "hdl_alloc");
    auto hdl = Builder.CreateIntrinsic(ptr_t, Intrinsic::coro_begin,
                                       {id, alloc}, nullptr, "hdl");
    auto suspend_0 = Builder.CreateIntrinsic(i8_t, Intrinsic::coro_suspend,
                                             {token_none, i1_false});
    auto _switch = Builder.CreateSwitch(suspend_0, suspend, 2);
    _switch->addCase(ConstantInt::get(i8_t, 0), &first_real_block);
    _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);

    // cleanup:
    Builder.SetInsertPoint(cleanup);
    auto mem = Builder.CreateIntrinsic(ptr_t, Intrinsic::coro_free, {id, hdl});
    Builder.CreateCall(utils::GenFreeCallee(M), {mem});
    Builder.CreateBr(suspend);

    // suspend:
    Builder.SetInsertPoint(suspend);
    auto unused = Builder.CreateIntrinsic(i1_t, Intrinsic::coro_end,
                                          {hdl, i1_false, token_none});
    Builder.CreateRet(hdl);

    for (int i = 0; auto &b : *F) {
      ++i;
      if (i < 4) {
        // We must skip [init, cleanup, suspend].
        continue;
      }

      for (auto it = b.begin(); it != b.end();) {
        auto &instr = *it;

        if (auto ret = dyn_cast<ReturnInst>(&instr)) {
          // Change terminating instructions.
          Builder.SetInsertPoint(ret);
          // TODO: functions which return void.
          auto call =
              Builder.CreateCall(set_ret_val, {promise, ret->getOperand(0)});
          ReplaceInstWithInst(&instr, BranchInst::Create(cleanup));
          it = call->getIterator();
          continue;

        } else if (auto call = dyn_cast<CallInst>(&instr)) {
          // Replace calls with coroutines clone calls.
          // Maybe need to generate coro clones in process.
          auto f = call->getCalledFunction();
          auto name = f->getName().str();
          if (IsCoroTarget(name)) {
            errs() << "Replace " << name << " call\n";
            auto coro_f = GenCoroFunc(f);
            Builder.SetInsertPoint(call);
            std::vector<Value *> args;
            for (int i = 0; i < call->arg_size(); ++i) {
              args.push_back(call->getArgOperand(i));
            }
            auto hdl = Builder.CreateCall(coro_f, args);
            // TODO: add suspension point here.
            // Scheduler must resume us when child coroutine is terminated.
            auto ret_val = Builder.CreateCall(get_ret_val, {promise});
            for (auto &use : call->uses()) {
              User *user = use.getUser();
              user->setOperand(use.getOperandNo(), ret_val);
            }
            call->eraseFromParent();
            it = ret_val->getIterator();
            continue;
          }
        }

        ++it;
      }
    }

    return F;
  }
};

namespace {

struct CoroGenPass : public PassInfoMixin<CoroGenPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    CoroGenerator gen{M};
    gen.Run();
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
