/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_CLIENT_PORT_ALLOCATOR_H_
#define LIB_COMPONENT_CLIENT_PORT_ALLOCATOR_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "senscord/osal.h"
#include "senscord/noncopyable.h"
#include "senscord/status.h"
#include "senscord/memory_allocator.h"
#include "./allocate_adapter.h"

namespace client {

// pre definition
class AllocateManager;

/**
 * @brief The allocation manager for each component port.
 */
class PortAllocator : private senscord::util::Noncopyable {
 public:
  /**
   * @brief Initialize the allocation for port.
   * @return Status object.
   */
  senscord::Status Init();

  /**
   * @brief De-Initialize the allocation
   * @return Status object.
   */
  senscord::Status Exit();

  /**
   * @brief Mapping to the virtual address in the process.
   * @param[in] (allocator_key) Allocator key.
   * @param[in] (serialized) Serialized rawdata message.
   * @param[out] (memory) Virtual memory informations.
   * @return Status object.
   */
  senscord::Status Mapping(
    const std::string& allocator_key,
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
   * @param[in] (manager) The parent manager.
   */
  explicit PortAllocator(AllocateManager* manager);

  /**
   * @brief Destructor
   */
  ~PortAllocator();

 private:
  // parent
  AllocateManager* manager_;

  // the adapter of allocator used in this component port.
  typedef std::map<std::string, AllocateAdapter*> AllocatorMap;
  AllocatorMap allocators_;
  senscord::osal::OSMutex* mutex_allocators_;
};

}   // namespace client
#endif  // LIB_COMPONENT_CLIENT_PORT_ALLOCATOR_H_
