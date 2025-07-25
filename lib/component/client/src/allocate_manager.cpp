/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "allocate_manager.h"
#include <inttypes.h>
#include <string>
#include <vector>
#include <utility>
#include "senscord/status.h"

namespace client {

/**
 * @brief Setup the adapters.
 * @param[in] (allocators) List of memory allocators.
 * @return Status object.
 */
senscord::Status AllocateManager::Init(
    const std::vector<senscord::MemoryAllocator*>& allocators) {
  senscord::osal::OSLockMutex(mutex_allocators_);
  typedef std::vector<senscord::MemoryAllocator*> AllocatorList;
  AllocatorList::const_iterator itr = allocators.begin();
  AllocatorList::const_iterator end = allocators.end();
  for (; itr != end; ++itr) {
    AllocateAdapter* adapter = new AllocateAdapter(*itr);
    allocators_.insert(std::make_pair((*itr)->GetKey(), adapter));
  }
  senscord::osal::OSUnlockMutex(mutex_allocators_);
  return senscord::Status::OK();
}

/**
 * @brief Release all adapters and allocators.
 * @return Status object.
 */
senscord::Status AllocateManager::Exit() {
  senscord::osal::OSLockMutex(mutex_port_allocators_);
  {
    PortAllocatorMap::const_iterator itr = port_allocators_.begin();
    PortAllocatorMap::const_iterator end = port_allocators_.end();
    for (; itr != end; ++itr) {
      delete itr->second;
    }
    port_allocators_.clear();
  }
  senscord::osal::OSUnlockMutex(mutex_port_allocators_);

  senscord::osal::OSLockMutex(mutex_allocators_);
  {
    AllocatorMap::const_iterator itr = allocators_.begin();
    AllocatorMap::const_iterator end = allocators_.end();
    for (; itr != end; ++itr) {
      delete itr->second;
    }
    allocators_.clear();
  }
  senscord::osal::OSUnlockMutex(mutex_allocators_);
  return senscord::Status::OK();
}

/**
 * @brief Open the port allocator.
 * @param[in] (port_id) Port ID of client component.
 * @return Status object.
 */
senscord::Status AllocateManager::Open(int32_t port_id) {
  senscord::osal::OSLockMutex(mutex_port_allocators_);
  PortAllocatorMap::const_iterator itr = port_allocators_.find(port_id);
  if (itr != port_allocators_.end()) {
    senscord::osal::OSUnlockMutex(mutex_port_allocators_);
    return SENSCORD_STATUS_FAIL("client", senscord::Status::kCauseAlreadyExists,
        "existed port id: %" PRId32, port_id);
  }

  PortAllocator* port_allocator = new PortAllocator(this);
  senscord::Status status = port_allocator->Init();
  SENSCORD_STATUS_TRACE(status);
  if (status.ok()) {
    port_allocators_.insert(std::make_pair(port_id, port_allocator));
  } else {
    delete port_allocator;
  }
  senscord::osal::OSUnlockMutex(mutex_port_allocators_);
  return status;
}

/**
 * @brief Close the port allocator.
 * @param[in] (port_id) Port ID of client component.
 * @return Status object.
 */
senscord::Status AllocateManager::Close(int32_t port_id) {
  senscord::Status status;
  senscord::osal::OSLockMutex(mutex_port_allocators_);
  PortAllocatorMap::iterator itr = port_allocators_.find(port_id);
  if (itr != port_allocators_.end()) {
    status = itr->second->Exit();
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      delete itr->second;
      port_allocators_.erase(itr);
    }
  }
  senscord::osal::OSUnlockMutex(mutex_port_allocators_);
  return status;
}

/**
 * @brief Mapping to the virtual address in the process.
 * @param[in] (port_id) Port ID of client component.
 * @param[in] (allocator_key) Allocator key.
 * @param[in] (serialized) Serialized rawdata message.
 * @param[out] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status AllocateManager::Mapping(
    int32_t port_id, const std::string& allocator_key,
    const std::vector<uint8_t>& serialized, senscord::MemoryContained* memory) {
  PortAllocator* port_allocator = NULL;
  senscord::Status status = GetPortAllocator(port_id, &port_allocator);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    return status;
  }

  // mapping with serialized rawdata.
  status = port_allocator->Mapping(allocator_key, serialized, memory);
  SENSCORD_STATUS_TRACE(status);
  return status;
}

/**
 * @brief Unmapping to the virtual address.
 * @param[in] (port_id) Port ID of client component.
 * @param[in] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status AllocateManager::Unmapping(
    int32_t port_id, const senscord::MemoryContained& memory) {
  PortAllocator* port_allocator = NULL;
  senscord::Status status = GetPortAllocator(port_id, &port_allocator);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    return status;
  }

  // mapping with serialized rawdata.
  status = port_allocator->Unmapping(memory);
  SENSCORD_STATUS_TRACE(status);
  return status;
}

/**
 * @brief Get the adapter for memory allocator.
 * @param[in] (allocator_key) Allocator key.
 * @param[out] (adapter) The adapter for memory allocator.
 * @return Status object.
 */
senscord::Status AllocateManager::GetAllocateAdapter(
    const std::string& allocator_key, AllocateAdapter** adapter) const {
  if (adapter == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "argument is null");
  }
  senscord::osal::OSLockMutex(mutex_allocators_);
  AllocatorMap::const_iterator itr = allocators_.find(allocator_key);
  if (itr == allocators_.end()) {
    senscord::osal::OSUnlockMutex(mutex_allocators_);
    return SENSCORD_STATUS_FAIL("client", senscord::Status::kCauseNotFound,
        "unknown allocator key: %s", allocator_key.c_str());
  }
  *adapter = itr->second;
  senscord::osal::OSUnlockMutex(mutex_allocators_);
  return senscord::Status::OK();
}

/**
 * @brief Get the allocate manager for component port.
 * @param[in] (port_id) Port ID.
 * @param[out] (port_allocator) The port allocate manager.
 * @return Status object.
 */
senscord::Status AllocateManager::GetPortAllocator(
    int32_t port_id, PortAllocator** port_allocator) const {
  if (port_allocator == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "argument is null");
  }
  senscord::osal::OSLockMutex(mutex_port_allocators_);
  PortAllocatorMap::const_iterator itr = port_allocators_.find(port_id);
  if (itr == port_allocators_.end()) {
    senscord::osal::OSUnlockMutex(mutex_port_allocators_);
    return SENSCORD_STATUS_FAIL("client", senscord::Status::kCauseNotFound,
        "unknown port id: %" PRId32, port_id);
  }
  *port_allocator = itr->second;
  senscord::osal::OSUnlockMutex(mutex_port_allocators_);
  return senscord::Status::OK();
}

/**
 * @brief Constructor
 */
AllocateManager::AllocateManager() {
  senscord::osal::OSCreateMutex(&mutex_allocators_);
  senscord::osal::OSCreateMutex(&mutex_port_allocators_);
}

/**
 * @brief Destructor
 */
AllocateManager::~AllocateManager() {
  senscord::osal::OSDestroyMutex(mutex_port_allocators_);
  senscord::osal::OSDestroyMutex(mutex_allocators_);
}

}   // namespace client
