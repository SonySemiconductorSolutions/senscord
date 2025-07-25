/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CORE_ALLOCATOR_MEMORY_ALLOCATOR_HEAP_H_
#define LIB_CORE_ALLOCATOR_MEMORY_ALLOCATOR_HEAP_H_

#include <vector>
#include "senscord/develop/memory_allocator_core.h"
#include "senscord/memory.h"

namespace senscord {

/**
 * @brief Memory allocator heap.
 */
class MemoryAllocatorHeap : public MemoryAllocatorCore {
 public:
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

#ifdef SENSCORD_SERVER
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
   *                      Must to be released with ReleaseMapping().
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
#endif  // SENSCORD_SERVER

  /**
   * @brief Whether the memory is shared.
   * @return True means sharing between other process, false means local.
   */
  virtual bool IsMemoryShared() const;

  /**
   * @brief Constructor.
   */
  MemoryAllocatorHeap() {}

  /**
   * @brief Destructor.
   */
  ~MemoryAllocatorHeap() {}
};

}   // namespace senscord
#endif  // LIB_CORE_ALLOCATOR_MEMORY_ALLOCATOR_HEAP_H_
