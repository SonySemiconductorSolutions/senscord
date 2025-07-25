/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "port_allocator.h"
#include <string>
#include <vector>
#include <utility>      // make_pair
#include "senscord/osal.h"
#include "senscord/status.h"
#include "senscord/logger.h"
#include "./allocate_manager.h"

namespace client {

/**
 * @brief Initialize the allocation for port.
 * @return Status object.
 */
senscord::Status PortAllocator::Init() {
  if (manager_ == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidOperation, "manager is null");
  }
  return senscord::Status::OK();
}

/**
 * @brief De-Initialize the allocation
 * @return Status object.
 */
senscord::Status PortAllocator::Exit() {
  while (!allocators_.empty()) {
    AllocatorMap::iterator itr = allocators_.begin();
    senscord::Status status = itr->second->Close();
    if (!status.ok()) {
      // print only
      SENSCORD_STATUS_TRACE(status);
      SENSCORD_LOG_ERROR("close allocation: %s", status.ToString().c_str());
    }
    allocators_.erase(itr);
  }
  return senscord::Status::OK();
}

/**
 * @brief Mapping to the virtual address in the process.
 * @param[in] (allocator_key) Allocator key.
 * @param[in] (serialized) Serialized rawdata message.
 * @param[out] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status PortAllocator::Mapping(
    const std::string& allocator_key,
    const std::vector<uint8_t>& serialized,
    senscord::MemoryContained* memory) {
  AllocateAdapter* adapter = NULL;
  AllocatorMap::iterator itr = allocators_.find(allocator_key);
  if (itr == allocators_.end()) {
    // if first open, get and open allocate adapter
    senscord::Status status = manager_->GetAllocateAdapter(allocator_key,
        &adapter);
    SENSCORD_STATUS_TRACE(status);
    if (!status.ok()) {
      return status;
    }

    status = adapter->Open();
    SENSCORD_STATUS_TRACE(status);
    if (!status.ok()) {
      return status;
    }

    allocators_.insert(std::make_pair(allocator_key, adapter));
  } else {
    adapter = itr->second;
  }

  // maping
  senscord::Status status = adapter->Mapping(serialized, memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unmapping to the virtual address.
 * @param[in] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status PortAllocator::Unmapping(
    const senscord::MemoryContained& memory) {
  std::string allocator_key = memory.memory->GetAllocator()->GetKey();
  AllocatorMap::iterator itr = allocators_.find(allocator_key);
  if (itr == allocators_.end()) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidOperation, "unknown allocator key: %s",
        allocator_key.c_str());
  }

  // umapping
  senscord::Status status = itr->second->Unmapping(memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Constructor
 * @param[in] (manager) The parent manager.
 */
PortAllocator::PortAllocator(AllocateManager* manager) : manager_(manager) {
  senscord::osal::OSCreateMutex(&mutex_allocators_);
}

/**
 * @brief Destructor
 */
PortAllocator::~PortAllocator() {
  senscord::osal::OSDestroyMutex(mutex_allocators_);
}

}   // namespace client
