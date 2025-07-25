/*
 * SPDX-FileCopyrightText: 2023 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_WAMR_WASM_ALLOCATOR_SRC_WASM_MEMORY_ALLOCATOR_H_
#define LIB_WAMR_WASM_ALLOCATOR_SRC_WASM_MEMORY_ALLOCATOR_H_

#include <stdint.h>
#include <vector>
#include "senscord/develop/memory_allocator_core.h"
#include "senscord/memory.h"

namespace senscord {

const char kArgumentWasmExecEnv[] = "wasm_exec_env";

/**
 * WASM Memory.
 */
class WasmMemory : public Memory {
 public:
  /**
   * @brief Constructor.
   * @param[in] (native_address) Native address.
   * @param[in] (wasm_address) WASM address.
   * @param[in] (size) Memory block size.
   * @param[in] (allocator) Depend allocator.
   */
  explicit WasmMemory(
      uintptr_t native_address, uint32_t wasm_address, uint32_t size,
      const MemoryAllocator& allocator)
      : native_address_(native_address), wasm_address_(wasm_address),
        size_(size), allocator_(const_cast<MemoryAllocator*>(&allocator)) {
  }

  /**
   * @brief Destructor.
   */
  virtual ~WasmMemory() {}

  /**
   * @brief Returns native address.
   * @return native address.
   */
  uintptr_t GetAddress() const {
    return native_address_;
  }

  /**
   * @brief Returns WASM address.
   * @return WASM address.
   */
  uint32_t GetWasmAddress() const {
    return wasm_address_;
  }

  /**
   * @brief Returns memory block size.
   * @return Memory block size.
   */
  size_t GetSize() const {
    return size_;
  }

  /**
   * @brief Invalidate a memory block.
   * @return Status object.
   */
  Status Invalidate() {
    Status status = allocator_->InvalidateCache(native_address_, size_);
    return SENSCORD_STATUS_TRACE(status);
  }

  /**
   * @brief Get depend allocator instance.
   * @return Allocator instance.
   */
  MemoryAllocator* GetAllocator() const {
    return allocator_;
  }

 private:
  uintptr_t native_address_;
  uint32_t wasm_address_;
  size_t size_;
  MemoryAllocator* allocator_;
};

/**
 * @brief WASM Memory allocator.
 */
class WasmMemoryAllocator : public MemoryAllocatorCore {
 public:
  /**
   * @brief Constructor.
   */
  WasmMemoryAllocator();

  /**
   * @brief Destructor.
   */
  ~WasmMemoryAllocator();

  /**
   * @brief Initialization process.
   * @param[in] (config) Allocator config.
   * @return Status object.
   */
  virtual Status Init(const AllocatorConfig& config);

  /**
   * @brief Termination process.
   * @return Status object.
   */
  virtual Status Exit();

  /**
   * @brief Allocate memory block.
   * @param[in]  (size) Size to allocate.
   * @param[out] (memory) Allocated Memory.
   * @return Status object.
   */
  virtual Status Allocate(size_t size, Memory** memory);

  /**
   * @brief Free memory block.
   * @param[in] (memory) Memory to free.
   * @return Status object.
   */
  virtual Status Free(Memory* memory);

  /**
   * @brief Serialize from contained memory area.
   * @param[in] (memory) Contained memory area information.
   * @param[out] (serialized) Serialized memory information.
   * @return Status object.
   */
  virtual Status Serialize(
    const MemoryContained& memory,
    std::vector<uint8_t>* serialized) const;

  /**
   * @brief Initialize the mapping area.
   * @return Status object.
   */
  virtual Status InitMapping();

  /**
   * @brief Deinitialize the mapping area.
   * @return Status object.
   */
  virtual Status ExitMapping();

  /**
   * @brief Mapping memory with serialized memory information.
   * @param[in] (serialized) Created from Serialize().
   * @param[out] (memory) Memory informations.
   *                      Must to be released with Unmapping().
   * @return Status object.
   */
  virtual Status Mapping(
    const std::vector<uint8_t>& serialized,
    MemoryContained* memory);

  /**
   * @brief Release the mapped area.
   * @param[in] (memory) Mapped memory.
   * @return Status object.
   */
  virtual Status Unmapping(const MemoryContained& memory);

  /**
   * @brief Whether the memory is shared.
   * @return Always returns false.
   */
  virtual bool IsMemoryShared() const;

 private:
  struct Impl;
  Impl* pimpl_;
};

}  // namespace senscord

#endif  // LIB_WAMR_WASM_ALLOCATOR_SRC_WASM_MEMORY_ALLOCATOR_H_
