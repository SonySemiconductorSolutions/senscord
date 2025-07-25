/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_CLIENT_ALLOCATE_ADAPTER_H_
#define LIB_COMPONENT_CLIENT_ALLOCATE_ADAPTER_H_

#include <stdint.h>
#include <vector>
#include "senscord/osal.h"
#include "senscord/noncopyable.h"
#include "senscord/status.h"
#include "senscord/memory_allocator.h"

namespace client {

/**
 * @brief The adapter of memory allocator.
 */
class AllocateAdapter : private senscord::util::Noncopyable {
 public:
  /**
   * @brief Open the memory allocator for mapping.
   * @return Status object.
   */
  senscord::Status Open();

  /**
   * @brief Close the memory allocator.
   * @return Status object.
   */
  senscord::Status Close();

  /**
   * @brief Mapping to the virtual address in the process.
   * @param[in] (serialized) Serialized rawdata message.
   * @param[out] (memory) Virtual memory informations.
   * @return Status object.
   */
  senscord::Status Mapping(
    const std::vector<uint8_t>& serialized,
    senscord::MemoryContained* memory);

  /**
   * @brief Unmapping to the virtual address.
   * @param[in] (memory) Virtual memory informations.
   * @return Status object.
   */
  senscord::Status Unmapping(const senscord::MemoryContained& memory);

  /**
   * @brief Constructor
   * @param[in] (allocator) Target memory allocator.
   */
  explicit AllocateAdapter(senscord::MemoryAllocator* allocator);

  /**
   * @brief Destructor
   */
  ~AllocateAdapter();

 private:
  // memory allocator
  senscord::MemoryAllocator* allocator_;

  // reference count
  uint32_t ref_count_;
  senscord::osal::OSMutex* mutex_;
};

}   // namespace client
#endif  // LIB_COMPONENT_CLIENT_ALLOCATE_ADAPTER_H_
