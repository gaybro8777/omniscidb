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

#include "Backend.h"

#include "QueryEngine/CodeGenerator.h"

namespace compiler {
std::shared_ptr<CompilationContext> CPUBackend::generateNativeCode(
    llvm::Function* func,
    llvm::Function* wrapper_func,
    const std::unordered_set<llvm::Function*>& live_funcs,
    const CompilationOptions& co) {
  return std::dynamic_pointer_cast<CpuCompilationContext>(
      CodeGenerator::generateNativeCPUCode(func, live_funcs, co));
}

CUDABackend::CUDABackend(Executor* executor,
                         bool is_gpu_smem_used,
                         CodeGenerator::GPUTarget& gpu_target)
    : executor_(executor), is_gpu_smem_used_(is_gpu_smem_used), gpu_target_(gpu_target) {
  const auto arch = gpu_target_.cuda_mgr->getDeviceArch();
  nvptx_target_machine_ = CodeGenerator::initializeNVPTXBackend(arch);
  gpu_target_.nvptx_target_machine = nvptx_target_machine_.get();
}

std::shared_ptr<CompilationContext> CUDABackend::generateNativeCode(
    llvm::Function* func,
    llvm::Function* wrapper_func,
    const std::unordered_set<llvm::Function*>& live_funcs,
    const CompilationOptions& co) {
  return std::dynamic_pointer_cast<GpuCompilationContext>(
      CodeGenerator::generateNativeGPUCode(executor_->get_extention_modules(),
                                           func,
                                           wrapper_func,
                                           live_funcs,
                                           is_gpu_smem_used_,
                                           co,
                                           gpu_target_));
}

std::shared_ptr<Backend> getBackend(ExecutorDeviceType dt,
                                    Executor* executor,
                                    bool is_gpu_smem_used_,
                                    CodeGenerator::GPUTarget& gpu_target) {
  switch (dt) {
    case ExecutorDeviceType::CPU:
      return std::make_shared<CPUBackend>();
    case ExecutorDeviceType::GPU:
      return std::make_shared<CUDABackend>(executor, is_gpu_smem_used_, gpu_target);
    default:
      CHECK(false);
      return {};
  };
}
}  // namespace compiler