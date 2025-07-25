/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_MEMORY_ALLOCATOR_H_
#define SENSCORD_MEMORY_ALLOCATOR_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "senscord/config.h"
#include "senscord/noncopyable.h"
#include "senscord/status.h"
#include "senscord/memory.h"

namespace senscord {

/**
 * @brief Memory allocator interface.
 */
class MemoryAllocator : private util::Noncopyable {
 public:
  /**
   * @brief Allocate memory block.
   * @param[in]  (size) Size to allocate.
   * @param[out] (memory) Allocated Memory.
   * @return Status object.
   */
  virtual Status Allocate(size_t size, Memory** memory) = 0;

  /**
   * @brief Free memory block.
   * @param[in] (memory) Memory to free.
   * @return Status object.
   */
  virtual Status Free(Memory* memory) = 0;

  /**
   * @brief Serialize from contained memory area.
   * @param[in] (memory) Contained memory area information.
   * @param[out] (serialized) Serialized memory information.
   * @return Status object.
   */
  virtual Status Serialize(
      const MemoryContained& memory,
      std::vector<uint8_t>* serialized) const {
    return Status::OK();
  }

  /**
   * @brief Initialize the mapping area.
   * @return Status object.
   */
  virtual Status InitMapping() {
    return Status::OK();
  }

  /**
   * @brief Deinitialize the mapping area.
   * @return Status object.
   */
  virtual Status ExitMapping() {
    return Status::OK();
  }

  /**
   * @brief Mapping memory with serialized memory information.
   * @param[in] (serialized) Created from Serialize().
   * @param[out] (memory) Memory informations.
   *                      Must to be released with ReleaseMapping().
   * @return Status object.
   */
  virtual Status Mapping(
      const std::vector<uint8_t>& serialized,
      MemoryContained* memory) {
    return Status::OK();
  }

  /**
   * @brief Release the mapped area.
   * @param[in] (memory) Mapped memory.
   * @return Status object.
   */
  virtual Status Unmapping(const MemoryContained& memory) {
    return Status::OK();
  }

  /**
   * @brief Invalidate of cache.
   * @param[in] (address) Start virtual address to invalidate.
   * @param[in] (size) Size to invalidate.
   * @return Status object.
   */
  virtual Status InvalidateCache(uintptr_t address, size_t size) = 0;

  /**
   * @brief Clean of cache.
   * @param[in] (address) Start virtual address to clean.
   * @param[in] (size) Size to clean.
   * @return Status object.
   */
  virtual Status CleanCache(uintptr_t address, size_t size) = 0;

  /**
   * @brief Get allocator key.
   * @return Allocator key.
   */
  virtual const std::string& GetKey() const = 0;

  /**
   * @brief Get allocator type.
   * @return Allocator type.
   */
  virtual const std::string& GetType() const = 0;

  /**
   * @brief Whether the memory is shared.
   * @return True means sharing between other process, false means local.
   */
  virtual bool IsMemoryShared() const = 0;

  /**
   * @brief Is cacheable allocator.
   * @return True is cacheable, false is not cacheable.
   */
  virtual bool IsCacheable() const = 0;

  /**
   * @brief Destructor.
   */
  virtual ~MemoryAllocator() {}
};

}  // namespace senscord
#endif  // SENSCORD_MEMORY_ALLOCATOR_H_
