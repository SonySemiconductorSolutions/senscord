/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wasm_memory_allocator.h"

#include <stdint.h>
#include <inttypes.h>

#include <string>
#include <map>
#include <vector>

#include "senscord/osal.h"
#include "senscord/logger.h"

#include "wasm_export.h"

/**
 * @brief Create allocator instance.
 * @return Created allocator instance. In case of failure, it returns NULL.
 */
extern "C" void* CreateAllocator() {
  return new senscord::WasmMemoryAllocator();
}

/**
 * @brief Destroy allocator instance.
 * @param[in] allocator  Instance created in CreateAllocator().
 */
extern "C" void DestroyAllocator(void* allocator) {
  delete reinterpret_cast<senscord::WasmMemoryAllocator*>(allocator);
}

namespace {

// status and log block name
const char kBlockName[] = "wasm";

/**
 * @brief Get the argument value.
 * @param[in] (args) argument list.
 * @param[in] (name) argument name.
 * @param[out] (value) argument value.
 * @return Status object.
 */
senscord::Status GetArgumentUint64(
    const std::map<std::string, std::string>& args,
    const std::string& name, uint64_t* value) {
  SENSCORD_STATUS_ARGUMENT_CHECK(value == NULL);
  // get value
  std::map<std::string, std::string>::const_iterator itr = args.find(name);
  if (itr == args.end()) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseNotFound,
        "no value is existed by %s", name.c_str());
  }
  const std::string& str = itr->second;
  // parse
  char* endptr = NULL;
  uint64_t num = 0;
  if ((senscord::osal::OSStrtoull(
      str.c_str(), &endptr, senscord::osal::kOSRadixAuto, &num) != 0) &&
      (endptr == NULL || *endptr != '\0')) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseInvalidArgument,
        "parse error: name=%s, value=%s", name.c_str(), str.c_str());
  }
  *value = num;
  return senscord::Status::OK();
}

/**
 * @brief Thread environment initializer.
 */
class WasmThreadEnv {
 public:
  explicit WasmThreadEnv(senscord::osal::OSMutex* mutex)
      : mutex_(mutex), thread_env_inited_() {
    senscord::osal::OSLockMutex(mutex_);
    if (!wasm_runtime_thread_env_inited()) {
      thread_env_inited_ = wasm_runtime_init_thread_env();
    }
  }

  ~WasmThreadEnv() {
    if (thread_env_inited_) {
      wasm_runtime_destroy_thread_env();
    }
    senscord::osal::OSUnlockMutex(mutex_);
  }

 private:
  senscord::osal::OSMutex* mutex_;
  bool thread_env_inited_;
};

}  // namespace

namespace senscord {

struct WasmMemoryAllocator::Impl {
  wasm_exec_env_t exec_env;
  senscord::osal::OSMutex* mutex;
};

WasmMemoryAllocator::WasmMemoryAllocator() : pimpl_(new Impl()) {
  senscord::osal::OSCreateMutex(&pimpl_->mutex);
}

WasmMemoryAllocator::~WasmMemoryAllocator() {
  senscord::osal::OSDestroyMutex(pimpl_->mutex);
  delete pimpl_;
}

/**
 * @brief Initialization process.
 * @param[in] (config) Allocator config.
 * @return Status object.
 */
Status WasmMemoryAllocator::Init(const AllocatorConfig& config) {
  MemoryAllocatorCore::Init(config);

  // Get "wasm_exec_env" and spwan it.
  {
    uint64_t value = 0;
    Status status = GetArgumentUint64(
        config.arguments, kArgumentWasmExecEnv, &value);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
    wasm_exec_env_t exec_env =
        reinterpret_cast<wasm_exec_env_t>(static_cast<uintptr_t>(value));
    pimpl_->exec_env = wasm_runtime_spawn_exec_env(exec_env);
    if (pimpl_->exec_env == NULL) {
      return SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseInvalidArgument,
          "wasm_runtime_spawn_exec_env(%p) failed", exec_env);
    }
    SENSCORD_LOG_INFO_TAGGED(
        kBlockName, "wasm_runtime_spawn_exec_env: %p (input=%p)",
        pimpl_->exec_env, exec_env);
  }

  return Status::OK();
}

/**
 * @brief Termination process.
 * @return Status object.
 */
Status WasmMemoryAllocator::Exit() {
  if (pimpl_->exec_env != NULL) {
    wasm_runtime_destroy_spawned_exec_env(pimpl_->exec_env);
    SENSCORD_LOG_INFO_TAGGED(
        kBlockName, "wasm_runtime_destroy_spawned_exec_env: %p",
        pimpl_->exec_env);
    pimpl_->exec_env = NULL;
  }
  return Status::OK();
}

/**
 * @brief Allocate memory block.
 * @param[in]  (size) Size to allocate.
 * @param[out] (memory) Allocated Memory.
 * @return Status object.
 */
Status WasmMemoryAllocator::Allocate(size_t size, Memory** memory) {
  SENSCORD_STATUS_ARGUMENT_CHECK(memory == NULL);
  uint32_t alloc_size = static_cast<uint32_t>(size);
  void* native_address = NULL;
  uint32_t wasm_address = 0;
  {
    WasmThreadEnv _env(pimpl_->mutex);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(pimpl_->exec_env);
    wasm_address = wasm_runtime_module_malloc(
        inst, alloc_size, &native_address);
  }
  if ((native_address == NULL) && (wasm_address == 0)) {
    return SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseResourceExhausted,
        "wasm_runtime_module_malloc(%" PRIu32 ") failed", alloc_size);
  }
  *memory = new WasmMemory(
      reinterpret_cast<uintptr_t>(native_address), wasm_address, alloc_size,
      *this);
  return Status::OK();
}

/**
 * @brief Free memory block.
 * @param[in] (memory) Memory to free.
 * @return Status object.
 */
Status WasmMemoryAllocator::Free(Memory* memory) {
  if (memory) {
    uint32_t wasm_address = static_cast<WasmMemory*>(memory)->GetWasmAddress();
    {
      WasmThreadEnv _env(pimpl_->mutex);
      wasm_module_inst_t inst = wasm_runtime_get_module_inst(pimpl_->exec_env);
      wasm_runtime_module_free(inst, wasm_address);
    }
    delete memory;
  }
  return Status::OK();
}

/**
 * @brief Serialize from contained memory area.
 * @param[in] (memory) Contained memory area information.
 * @param[out] (serialized) Serialized memory information.
 * @return Status object.
 */
Status WasmMemoryAllocator::Serialize(
    const MemoryContained& memory,
    std::vector<uint8_t>* serialized) const {
  return SENSCORD_STATUS_FAIL(
      kBlockName, Status::kCauseNotSupported, "not supported");
}

/**
 * @brief Initialize the mapping area.
 * @return Status object.
 */
Status WasmMemoryAllocator::InitMapping() {
  // do nothing
  return Status::OK();
}

/**
 * @brief Deinitialize the mapping area.
 * @return Status object.
 */
Status WasmMemoryAllocator::ExitMapping() {
  // do nothing
  return Status::OK();
}

/**
 * @brief Mapping memory with serialized memory information.
 * @param[in] (serialized) Created from Serialize().
 * @param[out] (memory) Memory informations.
 *                      Must to be released with Unmapping().
 * @return Status object.
 */
Status WasmMemoryAllocator::Mapping(
    const std::vector<uint8_t>& serialized,
    MemoryContained* memory) {
  SENSCORD_STATUS_ARGUMENT_CHECK(memory == NULL);
  // same as Allocate
  Status status = Allocate(serialized.size(), &memory->memory);
  SENSCORD_STATUS_TRACE(status);
  if (status.ok()) {
    memory->size = serialized.size();
    memory->offset = 0;
  }
  return status;
}

/**
 * @brief Release the mapped area.
 * @param[in] (memory) Mapped memory.
 * @return Status object.
 */
Status WasmMemoryAllocator::Unmapping(const MemoryContained& memory) {
  Status status = Free(memory.memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Whether the memory is shared.
 * @return Always returns false.
 */
bool WasmMemoryAllocator::IsMemoryShared() const {
  return false;
}

}  // namespace senscord
