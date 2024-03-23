#include <set>
#include <utility>
#include <vector>

#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "utils.h"

using namespace llvm;

using builder_t = IRBuilder<NoFolder>;
using fun_index_t = std::set<std::pair<StringRef, StringRef>>;

// Attributes.
const StringRef nonatomic_attr = "ltest_nonatomic";
const StringRef gen_attr = "ltest_gen";
const StringRef init_func_attr = "ltest_initfunc";
const StringRef target_attr_prefix = "ltesttarget_";

const StringRef coro_suf = "_coro";
const StringRef task_builder_suf = "_task_builder";

template <typename T>
void Assert(bool cond, const T &obj) {
  if (!cond) {
    errs() << "Assertion failed: \n";
    errs() << obj << "\n";
    assert(cond);
  }
}

void Assert(bool cond) { assert(cond); }

Value *GenerateCall(builder_t *builder, Function *fun) {
  if (fun->arg_size() == 0) {
    return builder->CreateCall(fun, {});
  }
  Assert(fun->arg_size() == 1, fun);
  auto arg = fun->arg_begin();
  Assert(arg->hasStructRetAttr(), fun);
  auto alloca = builder->CreateAlloca(arg->getType(), nullptr);
  return builder->CreateCall(fun, {alloca});
}

Twine ToCoro(const StringRef name) { return name + coro_suf; }

fun_index_t CreateFunIndex(const Module &M) {
  fun_index_t index{};
  for (auto it = M.global_begin(); it != M.global_end(); ++it) {
    if (it->getName() != "llvm.global.annotations") {
      continue;
    }
    auto *CA = dyn_cast<ConstantArray>(it->getOperand(0));
    for (auto o_it = CA->op_begin(); o_it != CA->op_end(); ++o_it) {
      auto *CS = dyn_cast<ConstantStruct>(o_it->get());
      auto *fun = dyn_cast<Function>(CS->getOperand(0));
      auto *AnnotationGL = dyn_cast<GlobalVariable>(CS->getOperand(1));
      auto annotation =
          dyn_cast<ConstantDataArray>(AnnotationGL->getInitializer())
              ->getAsCString();
      index.insert({annotation, fun->getName()});
    }
  }
  return index;
}

bool HasAttribute(const fun_index_t &index, const StringRef name,
                  const StringRef attr) {
  return index.find({attr, name}) != index.end();
}

// Generates
// * task_builders for target root tasks.
// * fill_ctx() where fill task builders and init funcs lists.
struct FillCtxGenerator final {
  const std::string fill_ctx_name = "fill_ctx";

  FillCtxGenerator(Module &M) : M(M) {
    auto &ctx = M.getContext();
    void_t = Type::getVoidTy(ctx);
    ptr_t = PointerType::get(ctx, 0);
    i32_t = Type::getInt32Ty(ctx);
    i8_t = Type::getInt8Ty(ctx);

    task_builder_list_t = ptr_t;
    task_builder_t = ptr_t;
    arg_list_t = ptr_t;
    init_func_list_t = ptr_t;

    push_task_builder_list = Function::Create(
        FunctionType::get(void_t, {task_builder_list_t, task_builder_t}, false),
        Function::ExternalLinkage, "push_task_builder_list", M);

    push_arg =
        Function::Create(FunctionType::get(void_t, {arg_list_t, i32_t}, false),
                         Function::ExternalLinkage, "push_arg", M);

    register_init_func = Function::Create(
        FunctionType::get(void_t, {init_func_list_t, ptr_t}, false),
        Function::ExternalLinkage, "register_init_func", M);
  }

  void run(const fun_index_t &index) {
    struct TargetCoro {
      // A pointer to the generated coroutine.
      Function *fun;
      // Real function name which will be saved in the promise.
      StringRef name;
    };

    // find args generators and target functions.
    std::unordered_map<Type *, Function *> generators;
    std::vector<TargetCoro> coroutines;
    for (const auto &[attr, symbol] : index) {
      if (attr.starts_with(target_attr_prefix)) {
        auto real_name = attr.slice(target_attr_prefix.size(), attr.size());
        auto coro = M.getFunction(ToCoro(symbol).str());
        Assert(coro != nullptr,
               "The coroutine was not generated for target: " + real_name);
        coroutines.emplace_back(TargetCoro{.fun = coro, .name = real_name});
        continue;
      }

      if (attr != gen_attr) {
        continue;
      }
      auto fun = M.getFunction(symbol);
      Assert(fun);
      if (fun->arg_size() == 0) {
        generators[fun->getReturnType()] = fun;
        continue;
      }
      errs() << fun->getName()
             << " is not valid generator: it must have 0 arguments\n";
    }

    auto &ctx = M.getContext();

    // void fill_ctx(TaskBuilderList, InitFuncList);
    auto fill_ctx_fun = Function::Create(
        FunctionType::get(void_t, {task_builder_list_t, init_func_list_t},
                          false),
        Function::ExternalLinkage, fill_ctx_name, M);
    auto block = BasicBlock::Create(ctx, "entry", fill_ctx_fun);
    builder_t Builder(block);

    // Fill task builders.
    auto task_builder_list = fill_ctx_fun->getArg(0);
    for (const auto &coro : coroutines) {
      auto builder_fun = GenTaskBuilder(coro.fun, coro.name, generators);
      if (builder_fun != nullptr) {
        Builder.CreateCall(push_task_builder_list,
                           {task_builder_list, builder_fun});
      }
    }

    // Fill init functions.
    auto init_func_list = fill_ctx_fun->getArg(1);
    for (const auto &it : index) {
      if (it.first != init_func_attr) {
        continue;
      }
      auto fun = M.getFunction(it.second);
      Assert(fun);
      if (fun->arg_size() != 0) {
        errs() << fun->getName()
               << " is not valid init func: it must have 0 arguments\n";
        continue;
      }
      Builder.CreateCall(register_init_func, {init_func_list, fun});
    }

    // ret void
    Builder.CreateRet(nullptr);
  }

 private:
  Function *GenTaskBuilder(
      Function *F, const StringRef name,
      const std::unordered_map<Type *, Function *> &generators) {
    assert(F != nullptr && "F is nullptr");

    auto arg_size = F->arg_size();
    if (arg_size < 1 || !F->getArg(0)->getType()->isPointerTy()) {
      errs() << "Generator must have `this` as first argument\n";
      return nullptr;
    }

    // Try to find generator for each argument except `this`.
    for (size_t i = 1; i < F->arg_size(); ++i) {
      auto arg_typ = F->getArg(i)->getType();
      if (generators.find(arg_typ) == generators.end()) {
        errs() << "Skip TaskBuilder generation for " << name
               << ": there is no generator for argument with type " << *arg_typ
               << "\n";
        return nullptr;
      }
    }

    errs() << "Generate TaskBuilder for " << F->getName() << "\n";
    auto &ctx = M.getContext();
    auto task_builder_name = name + task_builder_suf;

    // Task TaskBuilder(this, arg_list, name_ptr, hndl_ptr)
    auto ftype = FunctionType::get(void_t, {ptr_t, ptr_t, ptr_t, ptr_t}, false);

    Function *TaskBuilder = Function::Create(ftype, Function::ExternalLinkage,
                                             task_builder_name, M);
    auto this_arg = TaskBuilder->getArg(0);
    auto arg_list = TaskBuilder->getArg(1);
    auto name_ptr = TaskBuilder->getArg(2);
    auto hndl_ptr = TaskBuilder->getArg(3);

    auto block =
        BasicBlock::Create(ctx, "init", TaskBuilder, &TaskBuilder->front());
    builder_t Builder{block};

    // Generate arguments.
    std::vector<Value *> args;
    args.push_back(this_arg);
    for (size_t i = 1; i < F->arg_size(); ++i) {
      auto generator = generators.find(F->getArg(i)->getType())->second;
      auto arg = GenerateCall(&Builder,
                              generator);  // Builder.CreateCall(generator, {});
      args.push_back(arg);
      Builder.CreateCall(push_arg, {arg_list, arg});
    }

    auto hdl = Builder.CreateCall(F, args);
    Builder.CreateStore(hdl, hndl_ptr);

    // Declare global variable that holds the function name.
    auto name_const = createPrivateGlobalForString(M, name, false, "");
    auto generated_name_ptr = Builder.CreateGEP(
        ArrayType::get(i8_t, name.size() + 1), name_const,
        {ConstantInt::get(i32_t, 0), ConstantInt::get(i32_t, 0)});
    // char **name; *name = generate_name_ptr
    Builder.CreateStore(generated_name_ptr, name_ptr);

    // ret void
    Builder.CreateRet(nullptr);
    return TaskBuilder;
  }

  Module &M;
  PointerType *ptr_t;
  Type *void_t;
  Type *i32_t;
  Type *i8_t;

  PointerType *task_builder_t;
  PointerType *task_builder_list_t;
  Function *push_task_builder_list;

  PointerType *arg_list_t;
  Function *push_arg;

  PointerType *init_func_list_t;
  Function *register_init_func;
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
    promise_t = StructType::create("CoroPromise", i32_t, i32_t, ptr_t);

    promise_ptr_t = PointerType::get(promise_t, 0);
    task_builder_t = ptr_t;

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
    init_promise =
        Function::Create(FunctionType::get(void_t, {ptr_t}, false),
                         Function::ExternalLinkage, "init_promise", M);
    get_promise =
        Function::Create(FunctionType::get(promise_ptr_t, {ptr_t}, false),
                         Function::ExternalLinkage, "get_promise", M);
  }

  void Run(const fun_index_t &index) {
    for (auto &F : M) {
      if (IsCoroTarget(F.getName(), index)) {
        // Generate coroutine.
        GenCoroFunc(&F, index);
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
  PointerType *task_builder_t;

  Function *set_child_hdl;
  Function *set_ret_val;
  Function *get_ret_val;
  Function *get_promise;
  Function *init_promise;

  bool IsCoroTarget(const StringRef fun_name, const fun_index_t &index) {
    return HasAttribute(index, fun_name, nonatomic_attr);
  }

  bool NeedInterrupt(Instruction *insn) {
    if (isa<LoadInst>(insn) || isa<StoreInst>(insn) || isa<CallInst>(insn) ||
        isa<AtomicRMWInst>(insn)) {
      return true;
    }
    return false;
  }

  Function *GenCoroFunc(Function *F, const fun_index_t &index) {
    if (F->empty()) {
      errs() << "Skip generation for " << F->getName() << ": it's empty\n";
      return nullptr;
    }
    auto coro_name = ToCoro(F->getName());
    if (auto func = M.getFunction(coro_name.str())) {
      // Was generated later.
      // TODO: what if this symbol is defined in user code?
      return func;
    }
    errs() << "Gen " << coro_name << "\n";
    auto old_ret_t = F->getReturnType();
    auto newF = utils::CloneFuncChangeRetType(F, ptr_t, coro_name);
    assert(newF != nullptr && "Generated function is nullptr");
    return RawGenCoroFunc(newF, old_ret_t, index);
  }

  // TODO: rewrite it using one pass.
  Function *RawGenCoroFunc(Function *F, Type *old_ret_t,
                           const fun_index_t &index) {
    auto int_gen = utils::SeqGenerator{};
    auto &ctx = M.getContext();

    auto i32_0 = ConstantInt::get(i32_t, 0);
    auto ptr_null = ConstantPointerNull::get(ptr_t);
    auto token_none = ConstantTokenNone::get(ctx);
    auto i1_false = ConstantInt::get(i1_t, false);

    assert(ptr_t == F->getReturnType() && "F must have ptr return type");
    F->setPresplitCoroutine();

    builder_t Builder(&*F->begin());

    // * Add suspension points after suitable operations.
    for (auto b_it = (*F).begin(); b_it != (*F).end();) {
      auto cb = b_it;
      std::optional<BasicBlock *> new_entry_block;

      auto current_block = BasicBlock::Create(
          ctx, "execution." + std::to_string(int_gen.next()));
      for (auto insn_it = cb->begin(); insn_it != cb->end();) {
        if (std::next(insn_it) == cb->end()) {
          // Leave terminate instruction in this block.
          break;
        }
        auto start = insn_it;
        while (insn_it != cb->end() && !NeedInterrupt(&*insn_it)) {
          ++insn_it;
        }
        if (insn_it == cb->end()) {
          // This is the last segment, leave them in this block.
          break;
        }
        auto new_block = BasicBlock::Create(
            ctx, "execution." + std::to_string(int_gen.next()), F, &*b_it);
        if (!new_entry_block.has_value()) {
          new_entry_block = new_block;
        }
        b_it = cb->getIterator();
        auto iter = insn_it++;

        // Move instructions segment to the new block.
        while (true) {
          std::optional<decltype(iter)> prev;
          if (iter != start) {
            prev = std::prev(iter);
          }
          iter->removeFromParent();
          iter->insertInto(new_block, new_block->begin());
          if (!prev.has_value()) break;
          iter = prev.value();
        }
        Builder.SetInsertPoint(new_block);
        Builder.CreateIntrinsic(i8_t, Intrinsic::coro_suspend,
                                {token_none, i1_false});
      }
      b_it++;
      cb->setName("terminator." + std::to_string(int_gen.next()));
      if (new_entry_block.has_value()) {
        // Replace all uses of the terminate block with new entry block.
        cb->replaceAllUsesWith(new_entry_block.value());
        // But, phi nodes still must refer to terminate block,
        // because they are real successors.
        cb->replaceSuccessorsPhiUsesWith(new_entry_block.value(), &*cb);
      }
      // TODO: remove after DEBUG
      // int_gen.num += 100;
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
        // Check if terminate instruction is `ret`.
        if (auto ret = dyn_cast<ReturnInst>(instr)) {
          Builder.SetInsertPoint(ret);
          if (old_ret_t == i32_t) {
            Builder.CreateCall(set_ret_val, {promise, ret->getOperand(0)});
          } else {
            // If function returns not int we set i32 as ret value.
            // TODO: Take hash if we can.
            Builder.CreateCall(set_ret_val, {promise, i32_0});
          }
          // We need to suspend after `ret` because
          // return value must be accessed before coroutine destruction.
          auto suspend_res = Builder.CreateIntrinsic(
              i8_t, Intrinsic::coro_suspend, {token_none, i1_false});
          auto _switch = Builder.CreateSwitch(suspend_res, suspend, 2);
          _switch->addCase(ConstantInt::get(i8_t, 0), cleanup);
          _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);
          instr->eraseFromParent();
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

      // Suspension point is generated after each call, so we
      // can think that call is last instruction in itself execution block.
      if (auto call = dyn_cast<CallInst>(insn)) {
        // Replace calls with coroutines clone calls.
        // Maybe need to generate coro clones in process.
        auto f = call->getCalledFunction();
        if (!f || !f->hasName()) {
          continue;
        }
        auto f_name = f->getName();
        if (IsCoroTarget(f_name, index)) {
          // TODO: remove after debug.
          errs() << "Replace " << f_name << " call\n";
          auto coro_f = GenCoroFunc(f, index);
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
          // %as_promise = call Promise* @get_promise(hdl)
          // %ret_val    = call i32 @get_ret(as_promise)
          // operate with %ret_val

          Builder.SetInsertPoint(&*next_block->begin());
          auto as_promise = Builder.CreateCall(get_promise, {hdl});
          auto ret_val = Builder.CreateCall(get_ret_val, {as_promise});
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
    auto fun_index = CreateFunIndex(M);

    CoroGenerator gen{M};
    gen.Run(fun_index);

    FillCtxGenerator main_gen{M};
    main_gen.run(fun_index);

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
