/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "allocate_adapter.h"
#include <vector>

namespace client {

/**
 * @brief Open the memory allocator for mapping.
 * @return Status object.
 */
senscord::Status AllocateAdapter::Open() {
  if (allocator_ == NULL) {
    return SENSCORD_STATUS_FAIL("client", senscord::Status::kCauseAborted,
        "allocator is null");
  }

  senscord::osal::OSLockMutex(mutex_);
  if (ref_count_ == 0) {
    // if first open, initialize
    senscord::Status status = allocator_->InitMapping();
    SENSCORD_STATUS_TRACE(status);
    if (!status.ok()) {
      senscord::osal::OSUnlockMutex(mutex_);
      return status;
    }
  }
  ++ref_count_;
  senscord::osal::OSUnlockMutex(mutex_);
  return senscord::Status::OK();
}

/**
 * @brief Close the memory allocator.
 * @return Status object.
 */
senscord::Status AllocateAdapter::Close() {
  senscord::Status status;
  senscord::osal::OSLockMutex(mutex_);
  if (ref_count_ > 0) {
    if (ref_count_ == 1) {
      // if last close, exit
      status = allocator_->ExitMapping();
      SENSCORD_STATUS_TRACE(status);
      if (!status.ok()) {
        senscord::osal::OSUnlockMutex(mutex_);
        return status;
      }
    }
    --ref_count_;
  }
  senscord::osal::OSUnlockMutex(mutex_);
  return status;
}

/**
 * @brief Mapping to the virtual address in the process.
 * @param[in] (serialized) Serialized rawdata message.
 * @param[out] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status AllocateAdapter::Mapping(
    const std::vector<uint8_t>& serialized,
    senscord::MemoryContained* memory) {
  senscord::Status status = allocator_->Mapping(serialized, memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unmapping to the virtual address.
 * @param[in] (memory) Virtual memory informations.
 * @return Status object.
 */
senscord::Status AllocateAdapter::Unmapping(
    const senscord::MemoryContained& memory) {
  senscord::Status status = allocator_->Unmapping(memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Constructor
 * @param[in] (allocator) Target memory allocator.
 */
AllocateAdapter::AllocateAdapter(senscord::MemoryAllocator* allocator)
    : allocator_(allocator), ref_count_(0) {
  senscord::osal::OSCreateMutex(&mutex_);
}

/**
 * @brief Destructor
 */
AllocateAdapter::~AllocateAdapter() {
  senscord::osal::OSDestroyMutex(mutex_);
}

}   // namespace client
