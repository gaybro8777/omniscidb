/*
    Copyright (c) 2022 Intel Corporation
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <llvm/IR/Value.h>
#include <memory>

// todo: remove
#include "QueryEngine/CodeGenerator.h"
#include "QueryEngine/Execute.h"

namespace compiler {

class Backend {
 public:
  virtual ~Backend(){};
  virtual std::shared_ptr<CompilationContext> generateNativeCode(
      llvm::Function* func,
      llvm::Function* wrapper_func,
      const std::unordered_set<llvm::Function*>& live_funcs,
      const CompilationOptions& co) = 0;
};

class CPUBackend : public Backend {
 public:
  CPUBackend() = default;
  std::shared_ptr<CompilationContext> generateNativeCode(
      llvm::Function* func,
      llvm::Function* wrapper_func /*ignored*/,
      const std::unordered_set<llvm::Function*>& live_funcs,
      const CompilationOptions& co) override;
};

class CUDABackend : public Backend {
 public:
  CUDABackend(Executor* executor,
              bool is_gpu_smem_used,
              CodeGenerator::GPUTarget& gpu_target);

  std::shared_ptr<CompilationContext> generateNativeCode(
      llvm::Function* func,
      llvm::Function* wrapper_func,
      const std::unordered_set<llvm::Function*>& live_funcs,
      const CompilationOptions& co) override;

 private:
  Executor* executor_;
  bool is_gpu_smem_used_;
  CodeGenerator::GPUTarget& gpu_target_;

  mutable std::unique_ptr<llvm::TargetMachine> nvptx_target_machine_;
};

std::shared_ptr<Backend> getBackend(ExecutorDeviceType dt,
                                    Executor* executor,
                                    bool is_gpu_smem_used_,
                                    CodeGenerator::GPUTarget& gpu_target);

}  // namespace compiler