#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Demangle/Demangle.h"

using namespace llvm;

namespace {

    // Useful guide: https://www.cs.cornell.edu/~asampson/blog/llvm.html
struct SwitchPass : public PassInfoMixin<SwitchPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        auto is_gcd = [](CallBase* call){
            auto *function = call->getCalledFunction();
            if (function == nullptr) {
                // It can be null in some cases(
                errs() << "something went wrong" << "\n";
                return false;
            }

            // This code will be executed after the front-end, so all names will be mangled
            std::string mangled_name = function->getName().str();
            std::string unmangled_name = llvm::demangle(mangled_name);

            // Validate the unmangled name
            if (unmangled_name.find("std::gcd<int, int>(int, int)") != std::string::npos) {
                errs() << "find gcd! " << unmangled_name << "\n";
                return true;
            }

            return false;
        };

        auto gcd_calls = filter_all_calls(M, is_gcd);
        if (gcd_calls.empty()) {
            errs() << "didn't find any gcd calls!" << "\n";
            return PreservedAnalyses::all();
        }

        // Have gcds, so must add the switch function to the table
        LLVMContext& Ctx = M.getContext();
        // TODO: Should I mangle function there?
        FunctionCallee switchFunc = M.getOrInsertFunction(
                "switch_func", Type::getVoidTy(Ctx));

        BasicBlock* first_block = std::get<0>(gcd_calls[0]);
        IRBuilder<> builder(first_block);
        for (auto& gcd_call: gcd_calls) {
            if (std::get<1>(gcd_call)->getNextNode() == nullptr) {
                continue;
            }

            builder.SetInsertPoint(std::get<1>(gcd_call)->getNextNode());
            builder.CreateCall(switchFunc);
            errs() << "add the switch!" << "\n";
        }

        return PreservedAnalyses::none();
    };

private:
    static std::vector<std::tuple<BasicBlock*, CallBase*>> filter_all_calls(Module &M, const std::function<bool(CallBase*)>& predicate) {
        std::vector<std::tuple<BasicBlock*, CallBase*>> filtered_calls;
        for (auto &F : M) {
            for (auto &B: F) {
                for (auto &I: B) {
                    // Is the instruction a function call/method invoke?
                    auto *call = dyn_cast<llvm::CallBase>(&I);
                    if (call == nullptr) {
                        continue;
                    }

                    if (predicate(call)) {
                        filtered_calls.emplace_back(&B, call);
                    }
                }
            }
        }
        return filtered_calls;
    }
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Switch pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(SwitchPass());
                });
        }
    };
}
