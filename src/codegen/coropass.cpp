#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "utils.h"

using builder_t = IRBuilder<NoFolder>;
static cl::opt<std::string> test_foo("test_foo",
                                     cl::desc("Specify test foo name"),
                                     cl::value_desc("function name"));

bool IsCoroTarget(const std::string &target) {
  return !target.ends_with("_coro");
}

std::string ToCoro(const std::string &name) { return name + "_coro"; }

// Generates main where calls test function.
// With specified coroutine tasks.
struct MainGenerator final {
  MainGenerator(Module &M) : M(M) {
    auto &ctx = M.getContext();
    void_t = Type::getVoidTy(ctx);
    ptr_t = PointerType::get(ctx, 0);
    i32_t = Type::getInt32Ty(ctx);

    test_func = Function::Create(FunctionType::get(void_t, {ptr_t}, false),
                                 Function::ExternalLinkage, "test_func", M);
  }

  void run(const std::vector<std::string> &coros_to_call) {
    auto &ctx = M.getContext();
    auto main = Function::Create(FunctionType::get(i32_t, {}, false),
                                 Function::ExternalLinkage, "main", M);
    auto block = BasicBlock::Create(ctx, "entry", main);
    builder_t Builder(block);

    for (const auto &coro_name : coros_to_call) {
      auto coro = M.getFunction(coro_name);
      assert(coro && "coro function doesn't exist");
      auto hdl = Builder.CreateCall(coro, {});
      Builder.CreateCall(test_func, {hdl});
    }
    Builder.CreateRet(ConstantInt::get(i32_t, 0));
  }

 private:
  Module &M;
  Function *test_func;
  PointerType *ptr_t;
  Type *void_t;
  Type *i32_t;
};

// Generates coro clones for functions in the module.
struct CoroGenerator final {
  CoroGenerator(Module &M) : M(M) {
    auto &ctx = M.getContext();
    i1_t = Type::getInt1Ty(ctx);
    i8_t = Type::getInt8Ty(ctx);
    i32_t = Type::getInt32Ty(ctx);
    ptr_t = PointerType::get(ctx, 0);
    void_t = Type::getVoidTy(ctx);
    token_t = Type::getTokenTy(ctx);
    // This signature must be as in runtime declaration.
    // TODO: validate this by some way.
    promise_t =
        StructType::create("CoroPromise", i32_t, i32_t, i32_t, i32_t, ptr_t);
    promise_ptr_t = PointerType::get(promise_t, 0);

    // Names clashes?
    set_child_hdl = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, ptr_t}, false),
        Function::ExternalLinkage, "set_child_hdl", M);
    set_ret_val = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, i32_t}, false),
        Function::ExternalLinkage, "set_ret_val", M);
    get_child_ret =
        Function::Create(FunctionType::get(i32_t, {promise_ptr_t}, false),
                         Function::ExternalLinkage, "get_child_ret", M);
    init_promise =
        Function::Create(FunctionType::get(void_t, {ptr_t}, false),
                         Function::ExternalLinkage, "init_promise", M);
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
  Function *get_child_ret;
  Function *init_promise;

  Function *GenCoroFunc(Function *F) {
    if (F->empty()) {
      errs() << "Skip generation for " << F->getName() << ": it's empty\n";
      return nullptr;
    }
    auto coro_name = ToCoro(F->getName().str());
    SmallVector<char, 10> vec;
    if (auto func = M.getFunction(coro_name)) {
      // Was generated later.
      // TODO: what if this symbol is defined in user code?
      return func;
    }
    errs() << "Gen " << coro_name << "\n";
    auto old_ret_t = F->getReturnType();
    auto newF = utils::CloneFuncChangeRetType(F, ptr_t, coro_name);
    assert(newF != nullptr && "Generated function is nullptr");
    errs() << "Generated: " << coro_name << "\n";
    return rawGenCoroFunc(newF, old_ret_t);
  }

  // TODO: rewrite it using one pass.
  Function *rawGenCoroFunc(Function *F, Type *old_ret_t) {
    auto int_gen = utils::SeqGenerator{};
    auto &ctx = M.getContext();

    auto i32_0 = ConstantInt::get(i32_t, 0);
    auto ptr_null = ConstantPointerNull::get(ptr_t);
    auto token_none = ConstantTokenNone::get(ctx);
    auto i1_false = ConstantInt::get(i1_t, false);

    assert(ptr_t == F->getReturnType() && "F must have ptr return type");
    F->setPresplitCoroutine();

    builder_t Builder(&*F->begin());

    // * Add suspension points after each operation.
    for (auto b_it = (*F).begin(); b_it != (*F).end();) {
      auto cb = b_it;
      std::optional<BasicBlock *> new_entry_block;

      for (auto insn_it = cb->begin(); insn_it != cb->end();) {
        if (std::next(insn_it) == cb->end()) {
          // Left terminate instruction in this block.
          break;
        }
        auto new_block = BasicBlock::Create(
            ctx, "execution." + std::to_string(int_gen.next()), F, &*b_it);
        if (!new_entry_block.has_value()) {
          new_entry_block = new_block;
        }
        b_it = cb->getIterator();
        Builder.SetInsertPoint(new_block);
        auto suspend_res = Builder.CreateIntrinsic(
            i8_t, Intrinsic::coro_suspend, {token_none, i1_false});
        auto insn = insn_it++;
        insn->moveBefore(suspend_res);
      }
      b_it++;

      cb->setName("terminator." + std::to_string(int_gen.next()));
      if (new_entry_block.has_value()) {
        cb->replaceAllUsesWith(new_entry_block.value());
      }
    }

    auto &first_real_block = F->front();
    auto suspend = BasicBlock::Create(ctx, "suspend", F, &F->front());
    auto cleanup = BasicBlock::Create(ctx, "cleanup", F, suspend);
    auto init = BasicBlock::Create(ctx, "init", F, cleanup);

    // Init:
    Builder.SetInsertPoint(init);
    auto promise = Builder.CreateAlloca(promise_t, nullptr, "promise");
    Builder.CreateCall(init_promise, {promise});
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

    // Cleanup:
    Builder.SetInsertPoint(cleanup);
    auto mem = Builder.CreateIntrinsic(ptr_t, Intrinsic::coro_free, {id, hdl});
    Builder.CreateCall(utils::GenFreeCallee(M), {mem});
    Builder.CreateBr(suspend);

    // Suspend:
    Builder.SetInsertPoint(suspend);
    auto unused = Builder.CreateIntrinsic(i1_t, Intrinsic::coro_end,
                                          {hdl, i1_false, token_none});
    Builder.CreateRet(hdl);

    // * Replace ret instructions with result memorization.
    // We memorize return value in promise.
    // * Create terminate instructions in execution blocks.
    // * Replace function calls with coroutine clone calls.
    for (auto &b : *F) {
      auto b_name = b.getName();

      if (!b_name.starts_with("execution") && !b_name.starts_with("init") &&
          !b_name.starts_with("cleanup") && !b_name.starts_with("suspend")) {
        auto instr = &*b.rbegin();
        if (auto ret = dyn_cast<ReturnInst>(instr)) {
          if (old_ret_t == i32_t) {
            // TODO: what if function return type is not int?
            Builder.SetInsertPoint(ret);
            auto call =
                Builder.CreateCall(set_ret_val, {promise, ret->getOperand(0)});
          }
          ReplaceInstWithInst(instr, BranchInst::Create(cleanup));
        }
      }

      if (!b_name.starts_with("execution")) {
        continue;
      }

      auto suspend_res = &*b.rbegin();
      Builder.SetInsertPoint(&b);
      auto _switch = Builder.CreateSwitch(suspend_res, suspend, 2);

      auto next_block = &*std::next(b.getIterator());
      _switch->addCase(ConstantInt::get(i8_t, 0), next_block);
      _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);

      auto insn = &*std::prev(suspend_res->getIterator());
      if (auto call = dyn_cast<CallInst>(insn)) {
        // Replace calls with coroutines clone calls.
        // Maybe need to generate coro clones in process.
        auto f = call->getCalledFunction();
        auto f_name = f->getName().str();
        if (IsCoroTarget(f_name)) {
          // TODO: remove after debug.
          errs() << "Replace " << f_name << " call\n";
          auto coro_f = GenCoroFunc(f);
          if (coro_f == nullptr) {
            // We can't generate this call because
            // can't generate coro clone for callee.
            continue;
          }

          Builder.SetInsertPoint(call);
          std::vector<Value *> args(call->arg_size());
          for (int i = 0; i < call->arg_size(); ++i) {
            args[i] = call->getArgOperand(i);
          }
          auto hdl = Builder.CreateCall(coro_f, args);

          Builder.SetInsertPoint(call);
          Builder.CreateCall(set_child_hdl, {promise, hdl});
          // %hdl = ptr call @call_coro()
          // %suspend = @llvm.coro.suspend()
          // switch
          //
          // In the next block:
          // %ret_val = call void @get_child_ret(promise, hdl)
          // operate with %ret_val

          Builder.SetInsertPoint(&*next_block->begin());
          auto ret_val = Builder.CreateCall(get_child_ret, {promise});
          for (auto &use : call->uses()) {
            User *user = use.getUser();
            user->setOperand(use.getOperandNo(), ret_val);
          }
          call->eraseFromParent();
        }
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
    if (test_foo != "") {
      MainGenerator main_gen{M};
      main_gen.run(std::vector<std::string>{ToCoro(test_foo)});
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
