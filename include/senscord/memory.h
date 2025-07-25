/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_MEMORY_H_
#define SENSCORD_MEMORY_H_

#include <stdint.h>

#include "senscord/config.h"
#include "senscord/noncopyable.h"
#include "senscord/status.h"

namespace senscord {

// pre-definition
class MemoryAllocator;

/**
 * @brief Memory interface.
 */
class Memory : private util::Noncopyable {
 public:
  /**
   * @brief Get memory block address.
   * @return Memory block address.
   */
  virtual uintptr_t GetAddress() const = 0;

  /**
   * @brief Get memory block size.
   * @return Memory block size.
   */
  virtual size_t GetSize() const = 0;

  /**
   * @brief Invalidate of memory block.
   * @return Status object.
   */
  virtual Status Invalidate() = 0;

  /**
   * @brief Get depend allocator instance.
   * @return Allocator instance.
   */
  virtual MemoryAllocator* GetAllocator() const = 0;

  /**
   * @brief Destructor.
   */
  virtual ~Memory() {}
};

/**
 * @brief The memory area of use.
 */
struct MemoryContained {
  Memory* memory;     /**< Allocated memory area. */
  size_t size;        /**< Size of data contained. */
  size_t offset;      /**< Offset size from top address of memory area. */
};

}  // namespace senscord
#endif  // SENSCORD_MEMORY_H_
