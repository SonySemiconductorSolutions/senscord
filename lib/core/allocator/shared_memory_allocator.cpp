/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "allocator/shared_memory_allocator.h"

#include <inttypes.h>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include "senscord/osal.h"
#include "allocator/shared_allocation_firstfit.h"
#include "allocator/shared_memory.h"
#include "util/autolock.h"
#include "util/senscord_utils.h"
#include "logger/logger.h"

#ifdef _WIN32
#include "allocator/shared_memory_object_windows.h"
#else
#include "allocator/shared_memory_object_linux.h"
#endif  // _WIN32

namespace senscord {

const uint32_t kMinBlockSize = 4096;
const char* kArgumentName = "name";
const char* kArgumentTotalSize = "total_size";
const char* kSharedMemoryNamePrefix = "senscord.";

/**
 * @brief Address information.
 */
struct SharedAddress {
  int32_t physical_address;  /**< top address of memory object. */
  int32_t allocated_size;    /**< allocated size. */
  int32_t offset;            /**< offset from top address. */
  int32_t size;              /**< actual size of data. */
};

/**
 * @brief Address information used in Serialize & Mapping API.
 */
struct SharedAddressInfo {
  SharedAddress address;     /**< address of memory object. */
  uint32_t checksum;         /**< check sum */
};

const size_t kSharedAddressInfoSize = sizeof(SharedAddress);

/**
 * @brief Make CRC32 table.
 */
const uint32_t* MakeCrc32Table() {
  static uint32_t table[256];
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int j = 0; j < 8; ++j) {
      c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
    }
    table[i] = c;
  }
  return table;
}

const uint32_t* kCrc32Table = MakeCrc32Table();

/**
 * @brief Calculate the checksum.
 * @param[in] buffer  Buffer pointer.
 * @param[in] size  Buffer size.
 * @return Checksum value.
 */
uint32_t CalcChecksum(const void* buffer, size_t size) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer);
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i) {
    c = kCrc32Table[(c ^ ptr[i]) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFF;
}

/**
 * @brief Check the serialized data.
 * @param[in] (data) Serialized data.
 * @param[out] (info) Pointer to a variable that stores address information.
 * @return True if the address information, false otherwise.
 */
bool CheckSerializedData(
    const std::vector<uint8_t>& data, SharedAddressInfo* info) {
  if (data.size() != sizeof(SharedAddressInfo)) {
    return false;
  }
  const SharedAddressInfo* addr =
      reinterpret_cast<const SharedAddressInfo*>(data.data());
  uint32_t checksum = CalcChecksum(addr, kSharedAddressInfoSize);
  if (checksum != addr->checksum) {
    return false;
  }
  *info = *addr;
  return true;
}

std::set<std::string> SharedMemoryAllocator::memory_names_;

/**
 * @brief Constructor.
 */
SharedMemoryAllocator::SharedMemoryAllocator() :
    total_size_(), block_size_(), object_(), method_() {
  object_ = CreateSharedMemoryObject();
}

/**
 * @brief Destructor.
 */
SharedMemoryAllocator::~SharedMemoryAllocator() {
  Exit();
  delete object_;
  object_ = NULL;
}

/**
 * @brief Initialization.
 * @param[in] (config) Allocator config.
 * @return Status object.
 */
Status SharedMemoryAllocator::Init(const AllocatorConfig& config) {
  Status status = MemoryAllocatorCore::Init(config);
  SENSCORD_STATUS_TRACE(status);

  arguments_ = config.arguments;

  if (status.ok()) {
    // parse argument.
    status = ParseArguments();
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // open memory object.
    status = object_->Open(memory_name_, total_size_);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      total_size_ = object_->GetTotalSize();
      SENSCORD_LOG_DEBUG(
          "[Shared memory] Init: name=%s, total_size=%" PRId32
          ", block_size=%" PRId32,
          memory_name_.c_str(), total_size_, block_size_);
    }
  }

  if (status.ok()) {
    method_ = new FirstFitAllocation();
    status = method_->Init(total_size_ / block_size_);
    SENSCORD_STATUS_TRACE(status);
  }

  return status;
}

/**
 * @brief Exiting.
 * @return Status object.
 */
Status SharedMemoryAllocator::Exit() {
  FreeAll();

  delete method_;
  method_ = NULL;

  // close memory object.
  object_->Close();

  return Status::OK();
}

/**
 * @brief Allocate memory block.
 * @param[in] (size) Size to allocate.
 * @param[out] (memory) Allocated Memory.
 * @return Status object.
 */
Status SharedMemoryAllocator::Allocate(size_t size, Memory** memory) {
  if (memory == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "memory == null");
  }
  if (size == 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "size == 0");
  }
  if (size > static_cast<size_t>(total_size_)) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "size is too large");
  }

  util::AutoLock lock(&memory_list_mutex_);

  // allocate.
  int32_t num = static_cast<int32_t>((size + block_size_ - 1) / block_size_);
  OffsetParam offset = {};
  Status status = method_->Allocate(num, &offset);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // map to memory.
  void* pointer = NULL;
  status = object_->Map(offset.offset * block_size_,
                        static_cast<int32_t>(size), &pointer);
  if (!status.ok()) {
    method_->Free(offset);
    return SENSCORD_STATUS_TRACE(status);
  }

  SharedMemory* shared_memory = new SharedMemory(
      reinterpret_cast<uintptr_t>(pointer),
      offset.offset * block_size_, size, this);
  *memory = shared_memory;

  MappingInfo info = {};
  info.offset = offset;
  info.allocation = true;
  memory_list_.insert(std::make_pair(shared_memory, info));

  SENSCORD_LOG_DEBUG(
      "[Shared memory] Allocate: phys=%" PRId32 ", size=%" PRIuS,
      shared_memory->GetPhysicalAddress(), shared_memory->GetSize());

  return Status::OK();
}

/**
 * @brief Free memory block.
 * @param[in] (memory) Memory to free.
 * @return Status object.
 */
Status SharedMemoryAllocator::Free(Memory* memory) {
  util::AutoLock lock(&memory_list_mutex_);
  std::map<SharedMemory*, MappingInfo>::iterator pos =
      memory_list_.find(static_cast<SharedMemory*>(memory));
  if (pos == memory_list_.end()) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "unmanaged object");
  }

  // unmap to memory.
  Status status = object_->Unmap(
      reinterpret_cast<void*>(memory->GetAddress()));

  if (status.ok() && pos->second.allocation) {
    // free.
    status = method_->Free(pos->second.offset);
    if (!status.ok()) {
      SENSCORD_LOG_WARNING(
          "[Shared memory] Free: %s", status.ToString().c_str());
      status = Status::OK();
    }
  }

  if (status.ok()) {
    SENSCORD_LOG_DEBUG(
        "[Shared memory] Free: phys=%" PRId32 ", size=%" PRIuS ", %s",
        pos->first->GetPhysicalAddress(), pos->first->GetSize(),
        pos->second.allocation ? "free" : "unmap");
    delete pos->first;
    memory_list_.erase(pos);
  }

  return status;
}

/**
 * @brief Serialize from contained memory area.
 * @param[in] (memory) Contained memory area information.
 * @param[out] (serialized) Serialized memory information.
 * @return Status object.
 */
Status SharedMemoryAllocator::Serialize(
    const MemoryContained& memory,
    std::vector<uint8_t>* serialized) const {
  if (serialized == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "serialized == null");
  }
  if (memory.memory == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "memory == null");
  }

  const SharedMemory* shared_memory =
      static_cast<SharedMemory*>(memory.memory);

  SharedAddressInfo addr = {};
  addr.address.physical_address = shared_memory->GetPhysicalAddress();
  addr.address.allocated_size = static_cast<int32_t>(shared_memory->GetSize());
  addr.address.offset = static_cast<int32_t>(memory.offset);
  addr.address.size = static_cast<int32_t>(memory.size);
  addr.checksum = CalcChecksum(&addr, kSharedAddressInfoSize);

  uint8_t* ptr = reinterpret_cast<uint8_t*>(&addr);
  serialized->assign(ptr, ptr + sizeof(addr));

  SENSCORD_LOG_DEBUG(
      "[Shared memory] Serialize: phys=%" PRId32 ", size=%" PRIuS,
      shared_memory->GetPhysicalAddress(), shared_memory->GetSize());

  return Status::OK();
}

/**
 * @brief Initialize the mapping area.
 * @return Status object.
 */
Status SharedMemoryAllocator::InitMapping() {
  // do nothing
  return Status::OK();
}

/**
 * @brief Deinitialize the mapping area.
 * @return Status object.
 */
Status SharedMemoryAllocator::ExitMapping() {
  FreeAll();
  return Status::OK();
}

/**
 * @brief Mapping memory with serialized memory information.
 * @param[in] (serialized) Created from Serialize().
 * @param[out] (memory) Memory informations.
 *                      Must to be released with ReleaseMapping().
 * @return Status object.
 */
Status SharedMemoryAllocator::Mapping(
    const std::vector<uint8_t>& serialized, MemoryContained* memory) {
  if (memory == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "invalid parameter");
  }

  // check serialized data.
  SharedAddressInfo info = {};
  if (!CheckSerializedData(serialized, &info)) {
    // allocate.
    Status status = Allocate(serialized.size(), &memory->memory);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }

    const SharedMemory* shared_memory =
        static_cast<SharedMemory*>(memory->memory);

    memory->offset = 0;
    memory->size = static_cast<size_t>(shared_memory->GetSize());

    SENSCORD_LOG_DEBUG(
        "[Shared memory] Mapping: phys=%" PRId32 ", size=%" PRIuS ", alloc",
        shared_memory->GetPhysicalAddress(), shared_memory->GetSize());

    return Status::OK();
  }

  // map to memory.
  util::AutoLock lock(&memory_list_mutex_);
  void* pointer = NULL;
  Status status = object_->Map(
      info.address.physical_address, info.address.allocated_size, &pointer);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  SharedMemory* shared_memory = new SharedMemory(
      reinterpret_cast<uintptr_t>(pointer),
      info.address.physical_address, info.address.allocated_size, this);
  memory->memory = shared_memory;
  memory->offset = info.address.offset;
  memory->size = info.address.size;

  MappingInfo mapping_info = {};
  mapping_info.offset.offset = info.address.physical_address / block_size_;
  mapping_info.offset.size =
      (info.address.allocated_size + block_size_ - 1) / block_size_;
  mapping_info.allocation = false;
  memory_list_.insert(std::make_pair(shared_memory, mapping_info));

  SENSCORD_LOG_DEBUG(
      "[Shared memory] Mapping: phys=%" PRId32 ", size=%" PRIuS,
      shared_memory->GetPhysicalAddress(), shared_memory->GetSize());

  return Status::OK();
}

/**
 * @brief Release the mapped area.
 * @param[in] (memory) Mapped memory.
 * @return Status object.
 */
Status SharedMemoryAllocator::Unmapping(const MemoryContained& memory) {
  Status status = Free(memory.memory);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Whether the memory is shared.
 * @return Always returns true.
 */
bool SharedMemoryAllocator::IsMemoryShared() const {
  return true;
}

/**
 * @brief Is cacheable allocator.
 * @return Always returns false.
 */
bool SharedMemoryAllocator::IsCacheable() const {
  return false;
}

/**
 * @brief Parse the arguments.
 * @return Status object.
 */
Status SharedMemoryAllocator::ParseArguments() {
  Status status;
  // parse argument. (name)
  std::string name;
  status = GetArgument(kArgumentName, &name);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  // check name. (length 1~64)
  if (name.empty() || name.size() > 64) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument,
        "The length of name is invalid. (%s)", name.c_str());
  }
  // check name. (A-Z,a-z,0-9,'.','-','_')
  for (std::string::const_iterator itr = name.begin(), end = name.end();
      itr != end; ++itr) {
    if (*itr >= 'A' && *itr <= 'Z') {
      continue;
    }
    if (*itr >= 'a' && *itr <= 'z') {
      continue;
    }
    if (*itr >= '0' && *itr <= '9') {
      continue;
    }
    if (itr != name.begin() && (*itr == '.' || *itr == '-' || *itr == '_')) {
      continue;
    }
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument,
        "name contains illegal characters. (%s)", name.c_str());
  }
  // check duplicate name.
  if (!CheckDuplicateName(name)) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidArgument, "name is duplicated. (%s)",
          name.c_str());
  }
  memory_name_ = kSharedMemoryNamePrefix + name;

  // parse argument. (total_size)
  int64_t total_size = 0;
  status = GetArgument(kArgumentTotalSize, &total_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  if (total_size <= 0 || total_size > INT32_MAX) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument,
        "total_size(%" PRId64 ") is out of range.", total_size);
  }

  // get block size.
  uint32_t block_size = object_->GetBlockSize();
  // round up block size.
  block_size_ = static_cast<int32_t>((std::max)(
      block_size,
      ((kMinBlockSize + block_size - 1) / block_size) * block_size));
  // round up total_size.
  int64_t total_size2 =
      ((total_size + block_size_ - 1) / block_size_) * block_size_;
  if (total_size2 > INT32_MAX) {
    total_size2 -= block_size_;
  }
  total_size_ = static_cast<int32_t>(total_size2);

  return status;
}

/**
 * @brief Get the argument value.
 * @param[in] (argument_name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status SharedMemoryAllocator::GetArgument(
    const std::string& argument_name, std::string* value) {
  std::map<std::string, std::string>::const_iterator itr =
      arguments_.find(argument_name);
  if (itr == arguments_.end()) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseNotFound, "argument name(%s) not found.",
        argument_name.c_str());
  }
  *value = itr->second;
  return Status::OK();
}

/**
 * @brief Get the argument value.
 * @param[in] (argument_name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status SharedMemoryAllocator::GetArgument(
    const std::string& argument_name, int64_t* value) {
  std::string str_value;
  Status status = GetArgument(argument_name, &str_value);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  if (!util::StrToInt64(str_value, value)) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "conversion to int64 failed. (%s=%s)",
        argument_name.c_str(), str_value.c_str());
  }
  return Status::OK();
}

/**
 * @brief Check for duplicate name.
 * @param[in] (name) The memory object name.
 * @return False if the name is duplicated.
 */
bool SharedMemoryAllocator::CheckDuplicateName(const std::string& name) {
  return memory_names_.insert(name).second;
}

/**
 * @brief Free all memory.
 */
void SharedMemoryAllocator::FreeAll() {
  util::AutoLock lock(&memory_list_mutex_);
  if (!memory_list_.empty()) {
    std::map<SharedMemory*, MappingInfo> list = memory_list_;
    for (std::map<SharedMemory*, MappingInfo>::const_iterator
        itr = list.begin(), end = list.end(); itr != end; ++itr) {
      Free(memory_list_.begin()->first);
    }
  }
}

}  // namespace senscord
