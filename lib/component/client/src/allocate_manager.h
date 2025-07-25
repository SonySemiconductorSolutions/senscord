/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_CLIENT_ALLOCATE_MANAGER_H_
#define LIB_COMPONENT_CLIENT_ALLOCATE_MANAGER_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "senscord/osal.h"
#include "senscord/status.h"
#include "senscord/noncopyable.h"
#include "senscord/memory_allocator.h"
#include "./allocate_adapter.h"
#include "./port_allocator.h"

namespace client {

/**
 * @brief The manager for memory allocators on the component.
 */
class AllocateManager : private senscord::util::Noncopyable {
 public:
  /**
   * @brief Setup the adapters.
   * @param[in] (allocators) List of memory allocators.
   * @return Status object.
   */
  senscord::Status Init(
    const std::vector<senscord::MemoryAllocator*>& allocators);

  /**
   * @brief Release all adapters and allocators.
   * @return Status object.
   */
  senscord::Status Exit();

  /**
   * @brief Open the port allocator.
   * @param[in] (port_id) Port ID of client component.
   * @return Status object.
   */
  senscord::Status Open(int32_t port_id);

  /**
   * @brief Close the port allocator.
   * @param[in] (port_id) Port ID of client component.
   * @return Status object.
   */
  senscord::Status Close(int32_t port_id);

  /**
   * @brief Mapping to the virtual address in the process.
   * @param[in] (port_id) Port ID of client component.
   * @param[in] (allocator_key) Allocator key.
   * @param[in] (serialized) Serialized rawdata message.
   * @param[out] (memory) Virtual memory informations.
   * @return Status object.
   */
  senscord::Status Mapping(
    int32_t port_id,
    const std::string& allocator_key,
    const std::vector<uint8_t>& serialized,
    senscord::MemoryContained* memory);

  /**
   * @brief Unmapping to the virtual address.
   * @param[in] (port_id) Port ID of client component.
   * @param[in] (memory) Virtual memory informations.
   * @return Status object.
   */
  senscord::Status Unmapping(
    int32_t port_id, const senscord::MemoryContained& memory);

  /**
   * @brief Get the adapter for memory allocator.
   * @param[in] (allocator_key) Allocator key.
   * @param[out] (adapter) The adapter for memory allocator.
   * @return Status object.
   */
  senscord::Status GetAllocateAdapter(
    const std::string& allocator_key, AllocateAdapter** adapter) const;

  /**
   * @brief Constructor
   */
  AllocateManager();

  /**
   * @brief Destructor
   */
  ~AllocateManager();

 private:
  /**
   * @brief Get the allocate manager for component port.
   * @param[in] (port_id) Port ID.
   * @param[out] (port_allocator) The port allocate manager.
   * @return Status object.
   */
  senscord::Status GetPortAllocator(
    int32_t port_id, PortAllocator** port_allocator) const;

  // the adapters for memory allocator.
  typedef std::map<std::string, AllocateAdapter*> AllocatorMap;
  AllocatorMap allocators_;
  senscord::osal::OSMutex* mutex_allocators_;

  // the allocate manager for each component port.
  typedef std::map<int32_t, PortAllocator*> PortAllocatorMap;
  PortAllocatorMap port_allocators_;
  senscord::osal::OSMutex* mutex_port_allocators_;
};

}   // namespace client
#endif  // LIB_COMPONENT_CLIENT_ALLOCATE_MANAGER_H_
