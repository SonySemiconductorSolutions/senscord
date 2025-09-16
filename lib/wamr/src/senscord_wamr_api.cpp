/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <pthread.h>

#include <utility>
#include <sstream>

#include "senscord/status.h"
#include "senscord/logger.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/c_api/senscord_c_api.h"
#include "senscord/c_api/property_wasm_types.h"
#include "c_api/c_common.h"
#include "c_api/c_config.h"
#include "configuration/configuration_core.h"
#include "stream/stream_core.h"
#include "frame/frame_core.h"
#include "frame/channel_core.h"
#include "util/resource_list.h"
#include "src/senscord_wamr_types.h"
#include "src/senscord_wamr_context.h"
#include "src/senscord_wamr_util.h"
#include "src/wasm_allocator_manager.h"
#include "src/wasm_memory_allocator.h"
#include "src/wasm_memory.h"

#include "wasm_export.h"

// TODO: Remove this include (wasm_cluster_is_thread_terminated)
#include "../libraries/thread-mgr/thread_manager.h"

/**
 * @brief Initializes the SensCord native library.
 */
extern "C" int init_native_lib(void) {
  int ret = senscord_context_init();
  if (ret == 0) {
    senscord::WasmAllocatorManager::CreateInstance();
  }
  return ret;
}

/**
 * @brief Exits the SensCord native library.
 */
extern "C" void deinit_native_lib(void) {
  senscord::WasmAllocatorManager::DeleteInstance();
  senscord_context_exit();
}

/**
 * @brief WASM native library initialization function.
 */
extern "C" int senscord_init_native_lib(void) {
  return init_native_lib();
}

/**
 * @brief WASM native library deinitialization function.
 */
extern "C" void senscord_deinit_native_lib(void) {
  deinit_native_lib();
}

namespace {

namespace c_api = senscord::c_api;

// allocator type (library name)
const char kAllocatorTypeWasm[] = "wasm_allocator";

// status and log block name
const char kBlockName[] = "wasm";

/**
 * @brief WASM address to native pointer.
 */
template<typename T>
T ToNativePointer(wasm_module_inst_t inst, wasm_addr_t address) {
  T ptr = NULL;
  if (address != 0) {
    ptr = reinterpret_cast<T>(wasm_runtime_addr_app_to_native(inst, address));
  }
  return ptr;
}

/**
 * @brief WASM blocking operation.
 */
class WasmBlockingOperation {
 public:
  explicit WasmBlockingOperation(
      wasm_exec_env_t exec_env, senscord_stream_t stream = 0)
      : exec_env_(), stream_(stream) {
    bool ret = wasm_runtime_begin_blocking_op(exec_env);
    if (ret) {
      exec_env_ = exec_env;
      if (stream_ != 0) {
        senscord_context_set_blocking_stream(
            exec_env_, stream_, SENSCORD_CONTEXT_OP_ENTER);
      }
    }
  }

  ~WasmBlockingOperation() {
    if (exec_env_ != NULL) {
      if (stream_ != 0) {
        senscord_context_set_blocking_stream(
            exec_env_, stream_, SENSCORD_CONTEXT_OP_EXIT);
      }
      wasm_runtime_end_blocking_op(exec_env_);
    }
  }

  bool GetResult() const {
    if (exec_env_ == NULL) {
      c_api::SetLastError(SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseAborted,
          "Blocking operation aborted."));
      return false;
    }
    return true;
  }

 private:
  wasm_exec_env_t exec_env_;
  senscord_stream_t stream_;
};

/* =============================================================
 * Status APIs
 * ============================================================= */

/** senscord_get_last_error_level */
enum senscord_error_level_t senscord_get_last_error_level_wrapper(
    wasm_exec_env_t exec_env) {
  return senscord_get_last_error_level();
}

/** senscord_get_last_error_cause */
enum senscord_error_cause_t senscord_get_last_error_cause_wrapper(
    wasm_exec_env_t exec_env) {
  return senscord_get_last_error_cause();
}

/** senscord_get_last_error_string */
int32_t senscord_get_last_error_string_wrapper(
    wasm_exec_env_t exec_env,
    enum senscord_status_param_t param,
    wasm_addr_t buffer_addr, wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_get_last_error_string(param, buffer, length);
}

/* =============================================================
 * Core APIs
 * ============================================================= */

/**
 * @brief Change allocator configuration.
 *
 * Find instance's allocator key="wasm" and change to the following:
 *   allocator name="type.port" (overwrite)
 *   allocator key="wasm.instance_name.type.port" (overwrite)
 *   allocator type="wasm_allocator"
 */
void ChangeAllocatorConfig(senscord_config_t config) {
  senscord::c_api::ConfigHandle* handle =
      senscord::c_api::ToPointer<senscord::c_api::ConfigHandle*>(config);
  senscord::ConfigurationCore* config_core =
      reinterpret_cast<senscord::ConfigurationCore*>(handle->config);
  senscord::CoreConfig core_config = config_core->GetConfig();

  // Get the `instance_name`, including the allocator key="wasm".
  std::map<std::string, senscord::ComponentInstanceConfig*> instance_list;
  for (std::vector<senscord::ComponentInstanceConfig>::iterator
      inst_itr = core_config.instance_list.begin(),
      inst_end = core_config.instance_list.end();
      inst_itr != inst_end; ++inst_itr) {
    senscord::ComponentInstanceConfig& inst = *inst_itr;
    for (std::map<std::string, std::string>::iterator
        alloc_itr = inst.allocator_key_list.begin(),
        alloc_end = inst.allocator_key_list.end();
        alloc_itr != alloc_end; ) {
      if (alloc_itr->second == "wasm") {  // key="wasm"
        instance_list.insert(std::make_pair(inst.instance_name, &inst));
        // Add unique key after deleting.
        inst.allocator_key_list.erase(alloc_itr++);
      } else {
        ++alloc_itr;
      }
    }
  }

  // Get the `type` and `port` that match the `instance_name`.
  for (std::vector<senscord::StreamSetting>::const_iterator
      stream_itr = core_config.stream_list.begin(),
      stream_end = core_config.stream_list.end();
      stream_itr != stream_end; ++stream_itr) {
    const senscord::StreamSetting& stream = *stream_itr;
    std::map<std::string, senscord::ComponentInstanceConfig*>::iterator
        inst_itr = instance_list.find(stream.radical_address.instance_name);
    if (inst_itr != instance_list.end()) {
      std::string allocator_name;
      {
        std::ostringstream buf;
        buf << stream.radical_address.port_type << '.'
            << stream.radical_address.port_id;
        allocator_name = buf.str();
      }
      std::string allocator_key;
      {
        std::ostringstream buf;
        buf << "wasm." << inst_itr->first << '.' << allocator_name;
        allocator_key = buf.str();
      }
      // Update instance's allocator.
      inst_itr->second->allocator_key_list.insert(
          std::make_pair(allocator_name, allocator_key));
      // Add allocator.
      senscord::AllocatorConfig allocator_config = {};
      allocator_config.key = allocator_key;
      allocator_config.type = kAllocatorTypeWasm;
      allocator_config.cacheable = false;
      allocator_config.arguments["stream_key"] = stream.stream_key;
      core_config.allocator_list.push_back(allocator_config);
      SENSCORD_LOG_INFO_TAGGED(
          kBlockName, "wasm allocator: key=%s, name=%s",
          allocator_key.c_str(), allocator_name.c_str());
    }
  }

  config_core->SetConfig(core_config);
}

/**
 * @brief Initialize core.
 */
int32_t InitCore(
    wasm_exec_env_t exec_env,
    wasm_addr_t core_addr,
    senscord_config_t config) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_core_t* core = ToNativePointer<senscord_core_t*>(inst, core_addr);
  int32_t ret = 0;

  ChangeAllocatorConfig(config);

  ret = senscord_core_init_with_config(core, config);
  if (ret == 0) {
    ret = senscord_context_set_core(
        exec_env, *core, SENSCORD_CONTEXT_OP_ENTER);
    if (ret != 0) {
      senscord::Status status = *c_api::GetLastError();
      senscord_core_exit(*core);
      c_api::SetLastError(status);
    }
  }
  return ret;
}

/** senscord_core_init */
int32_t senscord_core_init_wrapper(
    wasm_exec_env_t exec_env,
    wasm_addr_t core_addr) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  senscord_config_t config = 0;
  int32_t ret = senscord_config_create(&config);
  if (ret == 0) {
    ret = InitCore(exec_env, core_addr, config);
    senscord_config_destroy(config);
  }
  return ret;
}

/** senscord_core_init_with_config */
int32_t senscord_core_init_with_config_wrapper(
    wasm_exec_env_t exec_env,
    wasm_addr_t core_addr,
    senscord_config_t config) {
  SENSCORD_C_API_ARGUMENT_CHECK(config == 0);
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return InitCore(exec_env, core_addr, config);
}

/** senscord_core_exit */
int32_t senscord_core_exit_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  int32_t ret = senscord_core_exit(core);
  if (ret == 0) {
    ret = senscord_context_set_core(exec_env, core, SENSCORD_CONTEXT_OP_EXIT);
  }
  return ret;
}

/** senscord_core_get_stream_count */
int32_t senscord_core_get_stream_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_core_get_stream_count(core, count);
}

/** senscord_core_get_stream_info */
int32_t senscord_core_get_stream_info_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    uint32_t index,
    wasm_addr_t stream_info_addr) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_core_get_stream_info() is not supported."));
  return -1;
}

/** senscord_core_get_stream_info_string */
int32_t senscord_core_get_stream_info_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    uint32_t index,
    enum senscord_stream_info_param_t param,
    wasm_addr_t buffer_addr, wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_core_get_stream_info_string(
      core, index, param, buffer, length);
}

/** senscord_core_get_opened_stream_count */
int32_t senscord_core_get_opened_stream_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    const char* stream_key,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_core_get_opened_stream_count(core, stream_key, count);
}

/** senscord_core_get_version */
int32_t senscord_core_get_version_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    wasm_addr_t version_addr) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_core_get_version() is not supported."));
  return -1;
}

/** @brief Resource for stream allocator */
const char kWasmStreamAllocator[] = "wasm_stream_allocator";

struct WasmStreamAllocator : public senscord::ResourceData {
  std::string stream_key;
  wasm_module_inst_t module_inst;

  WasmStreamAllocator() : stream_key(), module_inst() {}

  ~WasmStreamAllocator() {
    senscord::WasmAllocatorManager::GetInstance()->UnregisterWasm(
        stream_key, module_inst);
  }
};

/** senscord_core_open_stream_with_setting */
int32_t senscord_core_open_stream_with_setting_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    const char* stream_key,
    wasm_addr_t setting_addr,
    wasm_addr_t stream_addr) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  const senscord_open_stream_setting_t* setting =
      ToNativePointer<senscord_open_stream_setting_t*>(inst, setting_addr);
  senscord_stream_t* stream =
      ToNativePointer<senscord_stream_t*>(inst, stream_addr);
  int32_t ret = senscord_core_open_stream_with_setting(
      core, stream_key, setting, stream);
  if (ret == 0) {
    senscord::Status status =
        senscord::WasmAllocatorManager::GetInstance()->RegisterWasm(
            stream_key, inst);
    if (status.ok()) {
      senscord_context_set_stream(
          exec_env, *stream, core, SENSCORD_CONTEXT_OP_ENTER);
      // for UnregisterWasm
      senscord::StreamCore* stream_ptr =
          c_api::ToPointer<senscord::StreamCore*>(*stream);
      WasmStreamAllocator* wasm_stream_allocator =
          stream_ptr->GetResources()->Create<WasmStreamAllocator>(
              kWasmStreamAllocator);
      wasm_stream_allocator->stream_key = stream_key;
      wasm_stream_allocator->module_inst = inst;
    } else {
      senscord_core_close_stream(core, *stream);
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      ret = -1;
    }
  }
  return ret;
}

/** senscord_core_open_stream */
int32_t senscord_core_open_stream_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    const char* stream_key,
    wasm_addr_t stream_addr) {
  return senscord_core_open_stream_with_setting_wrapper(
      exec_env, core, stream_key, 0, stream_addr);
}

/** senscord_core_close_stream */
int32_t senscord_core_close_stream_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    senscord_stream_t stream) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  int32_t ret = senscord_core_close_stream(core, stream);
  if (ret == 0) {
    senscord_context_set_stream(
        exec_env, stream, core, SENSCORD_CONTEXT_OP_EXIT);
  }
  return ret;
}

/* =============================================================
 * Stream APIs
 * ============================================================= */

/**
 * @brief Check the stream allocator.
 */
bool CheckStreamAllocator(
    wasm_exec_env_t exec_env, senscord_stream_t stream) {
  SENSCORD_C_API_ARGUMENT_CHECK(stream == 0);
  const char* stream_key = senscord_stream_get_key(stream);
  SENSCORD_C_API_ARGUMENT_CHECK(stream_key == NULL);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord::WasmAllocatorState state =
      senscord::WasmAllocatorManager::GetInstance()->GetAllocatorState(
          stream_key, inst);
  if (state == senscord::kNotOwnedWasm) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCausePermissionDenied,
        "Stream API is restricted."));
    return false;
  }
  return true;
}

/** senscord_stream_start */
int32_t senscord_stream_start_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  int32_t ret = senscord_context_set_stream_running(
      exec_env, stream, SENSCORD_CONTEXT_OP_ENTER);
  if (ret == 0) {
    ret = senscord_stream_start(stream);
    if (ret != 0) {
      senscord::Status status = *c_api::GetLastError();
      senscord_context_set_stream_running(
          exec_env, stream, SENSCORD_CONTEXT_OP_EXIT);
      c_api::SetLastError(status);
    }
  }
  return ret;
}

/** senscord_stream_stop */
int32_t senscord_stream_stop_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  int32_t ret = senscord_stream_stop(stream);
  if (ret == 0) {
    senscord_context_set_stream_running(
        exec_env, stream, SENSCORD_CONTEXT_OP_EXIT);
  }
  return ret;
}

/** @brief Resource for frame memory */
const char kWasmFrameMemory[] = "wasm_frame_memory";

struct WasmFrameMemory : public senscord::ResourceData {
  senscord_frame_memory_t frame_memory;
  senscord_context_memory_t frame_type;
  senscord_context_memory_t user_data;

  WasmFrameMemory() : frame_memory(), frame_type(), user_data() {}

  ~WasmFrameMemory() {
    if (frame_memory != 0) {
      senscord_context_release_frame_memory(frame_memory);
    }
    if (frame_type != 0) {
      senscord_context_free_memory(frame_type);
    }
    if (user_data != 0) {
      senscord_context_free_memory(user_data);
    }
  }
};

/** senscord_stream_get_frame */
int32_t senscord_stream_get_frame_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t frame_addr,
    int32_t timeout_msec) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env, stream);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_frame_t* frame =
      ToNativePointer<senscord_frame_t*>(inst, frame_addr);
  int32_t ret = senscord_stream_get_frame(stream, frame, timeout_msec);
  if (ret == 0) {
    senscord_frame_memory_t frame_memory = 0;
    ret = senscord_context_reserve_frame_memory(
        exec_env, *frame, &frame_memory);
    if (ret == 0) {
      senscord::FrameCore* frame_ptr =
          c_api::ToPointer<senscord::FrameCore*>(*frame);
      WasmFrameMemory* wasm_frame_memory =
          frame_ptr->GetResources()->Create<WasmFrameMemory>(
              kWasmFrameMemory);
      wasm_frame_memory->frame_memory = frame_memory;
    }
  }
  return ret;
}


/** senscord_stream_release_frame */
int32_t senscord_stream_release_frame_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    senscord_frame_t frame) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return senscord_stream_release_frame(stream, frame);
}

/** senscord_stream_release_frame_unused */
int32_t senscord_stream_release_frame_unused_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    senscord_frame_t frame) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return senscord_stream_release_frame_unused(stream, frame);
}

/** senscord_stream_clear_frames */
int32_t senscord_stream_clear_frames_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t frame_number_addr) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  int32_t* frame_number = ToNativePointer<int32_t*>(inst, frame_number_addr);
  return senscord_stream_clear_frames(stream, frame_number);
}

/** senscord_stream_get_property */
int32_t senscord_stream_get_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* property_key,
    wasm_addr_t value_addr,
    wasm_size_t value_size) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  if ((strcmp(property_key, SENSCORD_WASM_MEMORY_POOL_PROPERTY_KEY) == 0) &&
      (value_size == sizeof(senscord_wasm_memory_pool_property_t))) {
    senscord_wasm_memory_pool_property_t* memory_pool =
        ToNativePointer<senscord_wasm_memory_pool_property_t*>(
            inst, value_addr);
    SENSCORD_C_API_ARGUMENT_CHECK(memory_pool == NULL);
    memory_pool->num = 0;
    memory_pool->size = 0;
    senscord_wasm_memory_pool_info_t info = {};
    int32_t ret = senscord_context_get_memory_pool_info(
        exec_env, stream, &info);
    if (ret == 0) {
      memory_pool->num = info.num;
      memory_pool->size = info.size;
    }
    return ret;
  }
  void* value = ToNativePointer<void*>(inst, value_addr);
  return senscord_stream_get_property(stream, property_key, value, value_size);
}

/** senscord_stream_set_property */
int32_t senscord_stream_set_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* property_key,
    wasm_addr_t value_addr,
    wasm_size_t value_size) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  if ((strcmp(property_key, SENSCORD_WASM_MEMORY_POOL_PROPERTY_KEY) == 0) &&
      (value_size == sizeof(senscord_wasm_memory_pool_property_t))) {
    senscord_wasm_memory_pool_property_t* memory_pool =
        ToNativePointer<senscord_wasm_memory_pool_property_t*>(
            inst, value_addr);
    SENSCORD_C_API_ARGUMENT_CHECK(memory_pool == NULL);
    if (memory_pool->num != 0) {
      // num != 0 : enable memory pool
      // check MemoryAllocator
      const char* stream_key = senscord_stream_get_key(stream);
      SENSCORD_C_API_ARGUMENT_CHECK(stream_key == NULL);
      senscord::WasmAllocatorState state =
          senscord::WasmAllocatorManager::GetInstance()->GetAllocatorState(
              stream_key, inst);
      if (state != senscord::kNotWasm) {
        c_api::SetLastError(SENSCORD_STATUS_FAIL(
            kBlockName, senscord::Status::kCauseInvalidOperation,
            "Unsupported allocator."));
        return -1;
      }
      // check FrameBuffering
      senscord::StreamCore* stream_ptr =
          c_api::ToPointer<senscord::StreamCore*>(stream);
      senscord::FrameBuffering frame_buffering =
          stream_ptr->GetInitialSetting().frame_buffering;
      if (frame_buffering.buffering == senscord::kBufferingOn) {
        if ((frame_buffering.num > 0) &&
            (memory_pool->num > static_cast<uint32_t>(frame_buffering.num))) {
          memory_pool->num = static_cast<uint32_t>(frame_buffering.num);
        }
      }
    } else {
      // num == 0 : disable memory pool
      memory_pool->size = 0;
    }
    return senscord_context_set_memory_pool(
        exec_env, stream, memory_pool->num, memory_pool->size);
  }
  const void* value = ToNativePointer<void*>(inst, value_addr);
  return senscord_stream_set_property(stream, property_key, value, value_size);
}

/** senscord_stream_get_userdata_property */
int32_t senscord_stream_get_userdata_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t buffer_addr,
    wasm_size_t buffer_size) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  void* buffer = ToNativePointer<void*>(inst, buffer_addr);
  return senscord_stream_get_userdata_property(stream, buffer, buffer_size);
}

/** senscord_stream_set_userdata_property */
int32_t senscord_stream_set_userdata_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t buffer_addr,
    wasm_size_t buffer_size) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  const void* buffer = ToNativePointer<void*>(inst, buffer_addr);
  return senscord_stream_set_userdata_property(stream, buffer, buffer_size);
}

/** senscord_stream_get_property_count */
int32_t senscord_stream_get_property_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_stream_get_property_count(stream, count);
}

/** senscord_stream_get_property_key */
int32_t senscord_stream_get_property_key_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    uint32_t index,
    wasm_addr_t property_key_addr) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_stream_get_property_key() is not supported."));
  return -1;
}

/** senscord_stream_get_property_key_string */
int32_t senscord_stream_get_property_key_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    uint32_t index,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_stream_get_property_key_string(
      stream, index, buffer, length);
}

/** senscord_stream_lock_property */
int32_t senscord_stream_lock_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    int32_t timeout_msec) {
  WasmBlockingOperation blocking_op(exec_env, stream);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return senscord_stream_lock_property(stream, timeout_msec);
}

/** senscord_stream_lock_property_with_key */
int32_t senscord_stream_lock_property_with_key_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t keys_addr,
    uint32_t count,
    int32_t timeout_msec,
    wasm_addr_t lock_resource_addr) {
  WasmBlockingOperation blocking_op(exec_env, stream);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_property_lock_resource_t* lock_resource =
      ToNativePointer<senscord_property_lock_resource_t*>(
        inst, lock_resource_addr);
  std::vector<const char*> keys;
  keys.reserve(count);
  wasm_addr_t* wasm_keys = ToNativePointer<wasm_addr_t*>(inst, keys_addr);
  for (uint32_t i = 0; i < count; ++i) {
    if (wasm_keys != NULL) {
      keys.push_back(ToNativePointer<const char*>(inst, wasm_keys[i]));
    }
  }
  const char** keys_ptr = keys.empty() ? NULL : &keys[0];
  int32_t ret = senscord_stream_lock_property_with_key(
      stream, keys_ptr, count, timeout_msec, lock_resource);
  return ret;
}

/** senscord_stream_unlock_property */
int32_t senscord_stream_unlock_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  return senscord_stream_unlock_property(stream);
}

/** senscord_stream_unlock_property_by_resource */
int32_t senscord_stream_unlock_property_by_resource_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    senscord_property_lock_resource_t lock_resource) {
  return senscord_stream_unlock_property_by_resource(stream, lock_resource);
}

/** @brief Resource for frame callback. */
struct WasmFrameCallbackParam {
  wasm_exec_env_t exec_env;
  wasm_addr_t callback_addr;
  wasm_addr_t private_data;

  WasmFrameCallbackParam() : exec_env(), callback_addr(), private_data() {
  }

  ~WasmFrameCallbackParam() {
    if (exec_env != NULL) {
      wasm_runtime_destroy_spawned_exec_env(exec_env);
    }
  }
};

const char kWasmFrameCallback[] = "wasm_frame_callback";

struct WasmFrameCallback : public senscord::ResourceData {
  senscord::osal::OSMutex* mutex;
  WasmFrameCallbackParam* param;

  WasmFrameCallback() : mutex(), param() {
    senscord::osal::OSCreateMutex(&mutex);
  }

  ~WasmFrameCallback() {
    delete param;
    senscord::osal::OSDestroyMutex(mutex);
    mutex = NULL;
  }
};

/**
 * @brief Frame received callback function.
 */
void OnFrameReceived(
    senscord::Stream* stream, void* private_data) {
  WasmFrameCallbackParam* param =
      reinterpret_cast<WasmFrameCallbackParam*>(private_data);

  if (param->exec_env != NULL) {
    WasmThreadEnv _env;
    senscord_stream_t stream_handle = c_api::ToHandle(stream);
    uint32_t argv[3];
    // argv[0]-[1]: 64bit stream handle
    senscord::osal::OSMemcpy(
        &argv[0], sizeof(uint32_t) * 2,
        &stream_handle, sizeof(senscord_stream_t));
    argv[2] = param->private_data;
    bool ret = wasm_runtime_call_indirect(
        param->exec_env, param->callback_addr, 3, argv);
    if (!ret) {
      SENSCORD_LOG_ERROR_TAGGED(
          kBlockName, "failed to wasm_runtime_call_indirect()");
      // TODO: Not exported (wasm_cluster_is_thread_terminated)
      if (wasm_cluster_is_thread_terminated(param->exec_env)) {
        param->exec_env = NULL;
      }
    }
  }

  if (param->exec_env == NULL) {
    SENSCORD_LOG_WARNING_TAGGED(
        kBlockName, "Terminate the frame callback thread");
    pthread_exit(NULL);
  }
}

/** senscord_stream_register_frame_callback */
int32_t senscord_stream_register_frame_callback_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t callback_addr,
    wasm_addr_t private_data) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  SENSCORD_C_API_ARGUMENT_CHECK(callback_addr == 0);
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }

  wasm_exec_env_t spawned_exec_env = wasm_runtime_spawn_exec_env(exec_env);
  if (spawned_exec_env == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseResourceExhausted,
        "wasm_runtime_spawn_exec_env() failed."));
    return -1;
  }

  senscord::StreamCore* stream_ptr =
      c_api::ToPointer<senscord::StreamCore*>(stream);
  WasmFrameCallback* frame_callback =
      stream_ptr->GetResources()->Create<WasmFrameCallback>(
          kWasmFrameCallback);

  WasmFrameCallbackParam* param = new WasmFrameCallbackParam;
  param->exec_env = spawned_exec_env;
  param->callback_addr = callback_addr;
  param->private_data = private_data;

  {
    senscord::Status status = stream_ptr->RegisterFrameCallback(
        OnFrameReceived, param);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      delete param;
      return -1;
    }

    // Releases the old parameter and sets new parameter.
    LockGuard _lock(frame_callback->mutex);
    delete frame_callback->param;
    frame_callback->param = param;
  }

  return 0;
}

/** senscord_stream_unregister_frame_callback */
int32_t senscord_stream_unregister_frame_callback_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  if (!CheckStreamAllocator(exec_env, stream)) {
    return -1;
  }
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  senscord::StreamCore* stream_ptr =
      c_api::ToPointer<senscord::StreamCore*>(stream);
  WasmFrameCallback* frame_callback =
      stream_ptr->GetResources()->Get<WasmFrameCallback>(kWasmFrameCallback);

  if (frame_callback != NULL) {
    senscord::Status status = stream_ptr->UnregisterFrameCallback();
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
  }

  // Releases the registered parameter.
  stream_ptr->GetResources()->Release(kWasmFrameCallback);

  return 0;
}

/** @brief Resource for event callback. */
struct WasmEventCallbackParam {
  wasm_exec_env_t exec_env;
  wasm_addr_t callback_addr;
  wasm_addr_t callback_old_addr;
  wasm_addr_t private_data;

  WasmEventCallbackParam() :
      exec_env(), callback_addr(), callback_old_addr(), private_data() {
  }

  ~WasmEventCallbackParam() {
    if (exec_env != NULL) {
      wasm_runtime_destroy_spawned_exec_env(exec_env);
    }
  }
};

typedef std::map<std::string, WasmEventCallbackParam*> WasmEventCallbackList;

const char kWasmEventCallback[] = "wasm_event_callback";

struct WasmEventCallback : public senscord::ResourceData {
  senscord::osal::OSMutex* mutex;
  WasmEventCallbackList list;

  WasmEventCallback() : mutex(), list() {
    senscord::osal::OSCreateMutex(&mutex);
  }

  ~WasmEventCallback() {
    for (WasmEventCallbackList::const_iterator
        itr = list.begin(), end = list.end(); itr != end; ++itr) {
      delete itr->second;
    }
    senscord::osal::OSDestroyMutex(mutex);
    mutex = NULL;
  }
};

/**
 * @brief Event received callback function.
 */
void OnEventReceived(
    senscord::Stream* stream, const std::string& event_type,
    const senscord::EventArgument& args, void* private_data) {
  WasmEventCallbackParam* param =
      reinterpret_cast<WasmEventCallbackParam*>(private_data);

  if (param->exec_env != NULL) {
    WasmThreadEnv _env;
    bool ret = false;
    {
      wasm_module_inst_t inst = wasm_runtime_get_module_inst(param->exec_env);
      wasm_addr_t type_heap = wasm_runtime_module_dup_data(
          inst, event_type.c_str(), event_type.size() + 1);

      if (param->callback_addr != 0) {
        senscord_stream_t stream_handle = c_api::ToHandle(stream);
        senscord_event_argument_t event_handle = c_api::ToHandle(&args);
        uint32_t argv[6];
        // argv[0]-[1]: 64bit stream handle
        senscord::osal::OSMemcpy(
            &argv[0], sizeof(uint32_t) * 2,
            &stream_handle, sizeof(senscord_stream_t));
        argv[2] = type_heap;
        // argv[3]-[4]: 64bit event argument handle
        senscord::osal::OSMemcpy(
            &argv[3], sizeof(uint32_t) * 2,
            &event_handle, sizeof(senscord_event_argument_t));
        argv[5] = param->private_data;
        ret = wasm_runtime_call_indirect(
            param->exec_env, param->callback_addr, 6, argv);
      } else if (param->callback_old_addr != 0) {
        uint32_t argv[3];
        argv[0] = type_heap;
        argv[1] = 0;  // reserved
        argv[2] = param->private_data;
        ret = wasm_runtime_call_indirect(
            param->exec_env, param->callback_old_addr, 3, argv);
      }

      if (type_heap != 0) {
        wasm_runtime_module_free(inst, type_heap);
      }
    }
    if (!ret) {
      SENSCORD_LOG_ERROR_TAGGED(
          kBlockName, "failed to wasm_runtime_call_indirect()");
      // TODO: Not exported (wasm_cluster_is_thread_terminated)
      if (wasm_cluster_is_thread_terminated(param->exec_env)) {
        param->exec_env = NULL;
      }
    }
  }

  if (param->exec_env == NULL) {
    SENSCORD_LOG_WARNING_TAGGED(
        kBlockName, "Terminate the event callback thread");
    pthread_exit(NULL);
  }
}

/**
 * @brief Register event callback.
 */
int32_t RegisterEventCallback(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* event_type,
    wasm_addr_t callback_addr,
    wasm_addr_t callback_old_addr,
    wasm_addr_t private_data) {
  SENSCORD_C_API_ARGUMENT_CHECK(stream == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(event_type == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(
      callback_addr == 0 && callback_old_addr == 0);
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }

  wasm_exec_env_t spawned_exec_env = wasm_runtime_spawn_exec_env(exec_env);
  if (spawned_exec_env == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseResourceExhausted,
        "wasm_runtime_spawn_exec_env() failed."));
    return -1;
  }

  senscord::StreamCore* stream_ptr =
      c_api::ToPointer<senscord::StreamCore*>(stream);
  WasmEventCallback* event_callback =
      stream_ptr->GetResources()->Create<WasmEventCallback>(
          kWasmEventCallback);

  WasmEventCallbackParam* param = new WasmEventCallbackParam;
  param->exec_env = spawned_exec_env;
  param->callback_addr = callback_addr;
  param->callback_old_addr = callback_old_addr;
  param->private_data = private_data;

  {
    senscord::Status status = stream_ptr->RegisterEventCallback(
        event_type, OnEventReceived, param);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      delete param;
      return -1;
    }

    // Releases the old parameter and sets new parameter.
    LockGuard _lock(event_callback->mutex);
    std::pair<WasmEventCallbackList::iterator, bool> ret =
        event_callback->list.insert(std::make_pair(event_type, param));
    if (!ret.second) {
      delete ret.first->second;
      ret.first->second = param;
    }
  }

  return 0;
}

/** senscord_stream_register_event_callback */
int32_t senscord_stream_register_event_callback_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* event_type,
    wasm_addr_t callback_addr,
    wasm_addr_t private_data) {
  return RegisterEventCallback(
      exec_env, stream, event_type, 0, callback_addr, private_data);
}

/** senscord_stream_register_event_callback2 */
int32_t senscord_stream_register_event_callback2_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* event_type,
    wasm_addr_t callback_addr,
    wasm_addr_t private_data) {
  return RegisterEventCallback(
      exec_env, stream, event_type, callback_addr, 0, private_data);
}

/** senscord_stream_unregister_event_callback */
int32_t senscord_stream_unregister_event_callback_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    const char* event_type) {
  SENSCORD_C_API_ARGUMENT_CHECK(stream == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(event_type == NULL);
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  senscord::StreamCore* stream_ptr =
      c_api::ToPointer<senscord::StreamCore*>(stream);
  WasmEventCallback* event_callback =
      stream_ptr->GetResources()->Get<WasmEventCallback>(kWasmEventCallback);

  bool list_empty = false;
  if (event_callback != NULL) {
    senscord::Status status =
        stream_ptr->UnregisterEventCallback(event_type);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }

    // Releases the registered parameter.
    LockGuard _lock(event_callback->mutex);
    WasmEventCallbackList::iterator itr =
        event_callback->list.find(event_type);
    if (itr != event_callback->list.end()) {
      delete itr->second;
      event_callback->list.erase(itr);
    }
    list_empty = event_callback->list.empty();
  } else {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        kBlockName, senscord::Status::kCauseNotFound,
        "no registered event type: %s", event_type));
    return -1;
  }

  if (list_empty) {
    stream_ptr->GetResources()->Release(kWasmEventCallback);
  }

  return 0;
}

/* =============================================================
 * Frame APIs
 * ============================================================= */

/** senscord_frame_get_sequence_number */
int32_t senscord_frame_get_sequence_number_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    wasm_addr_t frame_number_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint64_t* frame_number =
      ToNativePointer<uint64_t*>(inst, frame_number_addr);
  return senscord_frame_get_sequence_number(frame, frame_number);
}

/** senscord_frame_get_type */
int32_t senscord_frame_get_type_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    wasm_addr_t type_wptr) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  wasm_addr_t* type_addr = ToNativePointer<wasm_addr_t*>(inst, type_wptr);
  SENSCORD_C_API_ARGUMENT_CHECK(type_addr == NULL);

  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  {
    // frame_type
    WasmFrameMemory* frame_memory =
        frame_ptr->GetResources()->Get<WasmFrameMemory>(kWasmFrameMemory);
    if (frame_memory->frame_type == 0) {
      const std::string& frame_type = frame_ptr->GetParentStream()->GetType();
      uint32_t type_size = static_cast<uint32_t>(frame_type.size() + 1);
      int32_t ret = senscord_context_duplicate_memory(
          exec_env, frame_type.c_str(), type_size, &frame_memory->frame_type);
      if (ret != 0) {
        return ret;
      }
    }
    *type_addr = senscord_context_get_wasm_address(frame_memory->frame_type);
  }
  return 0;
}

/** senscord_frame_get_channel_count */
int32_t senscord_frame_get_channel_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    wasm_addr_t channel_count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* channel_count =
      ToNativePointer<uint32_t*>(inst, channel_count_addr);
  return senscord_frame_get_channel_count(frame, channel_count);
}

/** @brief Resource for channel memory */
const char kWasmChannelMemory[] = "wasm_channel_memory";

struct WasmChannelMemory : public senscord::ResourceData {
  senscord_frame_t parent_frame;
  senscord_wasm_memory_area_t area;
  senscord_context_memory_t raw_data_type;
  uint64_t timestamp;

  WasmChannelMemory() : parent_frame(), area(), raw_data_type(), timestamp() {}

  ~WasmChannelMemory() {
    if (raw_data_type != 0) {
      senscord_context_free_memory(raw_data_type);
    }
  }
};

/** senscord_frame_get_channel */
int32_t senscord_frame_get_channel_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    uint32_t index,
    wasm_addr_t channel_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_channel_t* channel =
      ToNativePointer<senscord_channel_t*>(inst, channel_addr);
  int32_t ret = senscord_frame_get_channel(frame, index, channel);
  if (ret == 0) {
    senscord::ChannelCore* channel_ptr =
        c_api::ToPointer<senscord::ChannelCore*>(*channel);
    WasmChannelMemory* channel_memory =
        channel_ptr->GetResources()->Create<WasmChannelMemory>(
            kWasmChannelMemory);
    channel_memory->parent_frame = frame;
  }
  return ret;
}

/** senscord_frame_get_channel_from_channel_id */
int32_t senscord_frame_get_channel_from_channel_id_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    uint32_t channel_id,
    wasm_addr_t channel_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_channel_t* channel =
      ToNativePointer<senscord_channel_t*>(inst, channel_addr);
  int32_t ret = senscord_frame_get_channel_from_channel_id(
      frame, channel_id, channel);
  if (ret == 0) {
    senscord::ChannelCore* channel_ptr =
        c_api::ToPointer<senscord::ChannelCore*>(*channel);
    WasmChannelMemory* channel_memory =
        channel_ptr->GetResources()->Create<WasmChannelMemory>(
            kWasmChannelMemory);
    channel_memory->parent_frame = frame;
  }
  return ret;
}

/** senscord_frame_get_user_data */
int32_t senscord_frame_get_user_data_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    wasm_addr_t user_data_addr) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_user_data_wasm_t* user_data =
      ToNativePointer<senscord_user_data_wasm_t*>(inst, user_data_addr);
  SENSCORD_C_API_ARGUMENT_CHECK(user_data == NULL);

  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  senscord::Frame::UserData tmp_user_data = {};
  {
    senscord::Status status = frame_ptr->GetUserData(&tmp_user_data);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
  }

  if ((tmp_user_data.address != NULL) && (tmp_user_data.size != 0)) {
    WasmFrameMemory* frame_memory =
        frame_ptr->GetResources()->Get<WasmFrameMemory>(kWasmFrameMemory);
    if (frame_memory->user_data == 0) {
      int32_t ret = senscord_context_duplicate_memory(
          exec_env, tmp_user_data.address, tmp_user_data.size,
          &frame_memory->user_data);
      if (ret != 0) {
        return ret;
      }
    }
    user_data->address_addr =
        senscord_context_get_wasm_address(frame_memory->user_data);
    user_data->size = static_cast<wasm_size_t>(tmp_user_data.size);
  } else {
    user_data->address_addr = 0;
    user_data->size = 0;
  }

  return 0;
}

/* =============================================================
 * Channel APIs
 * ============================================================= */

/** senscord_channel_get_channel_id */
int32_t senscord_channel_get_channel_id_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t channel_id_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* channel_id = ToNativePointer<uint32_t*>(inst, channel_id_addr);
  return senscord_channel_get_channel_id(channel, channel_id);
}

int32_t senscord_channel_get_raw_data_handle_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t raw_data_addr) {

  SENSCORD_C_API_ARGUMENT_CHECK(channel == 0);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_raw_data_handle_t* raw_data =
      ToNativePointer<senscord_raw_data_handle_t*>(inst, raw_data_addr);
  SENSCORD_C_API_ARGUMENT_CHECK(raw_data == NULL);
  senscord::ChannelCore* channel_ptr =
      c_api::ToPointer<senscord::ChannelCore*>(channel);
  senscord::Channel::RawData tmp_raw_data = {};
  {
    senscord::Status status = channel_ptr->GetRawData(&tmp_raw_data);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
  }
  raw_data->address = reinterpret_cast<uint64_t>(tmp_raw_data.address);
  raw_data->size = static_cast<wasm_size_t>(tmp_raw_data.size);
  raw_data->timestamp = tmp_raw_data.timestamp;
  {
    // raw_data_type
    const char* kResourceRawDataType = "handle_data_type";
    uint32_t type_size = static_cast<uint32_t>(tmp_raw_data.type.size() + 1);
    
    // Note: WasmAllocatorManager in this context is used for stream-based allocation
    // For raw data type handling, we need to use a different approach
    // Since we cannot directly allocate memory for type string, we'll set it to null
    raw_data->type = nullptr;
  }
  return 0;
}


/** senscord_channel_get_raw_data */
int32_t senscord_channel_get_raw_data_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t raw_data_addr) {
  SENSCORD_C_API_ARGUMENT_CHECK(channel == 0);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_raw_data_wasm_t* raw_data =
      ToNativePointer<senscord_raw_data_wasm_t*>(inst, raw_data_addr);
  SENSCORD_C_API_ARGUMENT_CHECK(raw_data == NULL);

  senscord::ChannelCore* channel_ptr =
      c_api::ToPointer<senscord::ChannelCore*>(channel);
  WasmChannelMemory* channel_memory =
      channel_ptr->GetResources()->Create<WasmChannelMemory>(
          kWasmChannelMemory);
  if (channel_memory->area.memory == 0) {
    senscord::Channel::RawData tmp_raw_data = {};
    senscord::Status status = channel_ptr->GetRawData(&tmp_raw_data);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
    senscord::RawDataMemory rawdata_mem = {};
    channel_ptr->GetRawDataMemory(&rawdata_mem);
    std::string allocator_type;
    if (rawdata_mem.memory != NULL) {
      allocator_type = rawdata_mem.memory->GetAllocator()->GetType();
    }
    // memory, offset, size
    if (allocator_type == kAllocatorTypeWasm) {
      channel_memory->area.memory = c_api::ToHandle(rawdata_mem.memory);
      channel_memory->area.offset = static_cast<uint32_t>(rawdata_mem.offset);
      channel_memory->area.size = static_cast<uint32_t>(rawdata_mem.size);
    } else {
      int32_t ret = senscord_context_get_channel_memory(
          exec_env, channel_memory->parent_frame, channel,
          &channel_memory->area);
      if (ret != 0) {
        return ret;
      }
    }
    // raw data type
    if (channel_memory->raw_data_type == 0) {
      uint32_t type_size = static_cast<uint32_t>(tmp_raw_data.type.size() + 1);
      int32_t ret = senscord_context_duplicate_memory(
          exec_env, tmp_raw_data.type.c_str(), type_size,
          &channel_memory->raw_data_type);
      if (ret != 0) {
        return ret;
      }
    }
    // timestamp
    channel_memory->timestamp = tmp_raw_data.timestamp;
  }
  senscord::WasmMemory* wasm_memory =
      c_api::ToPointer<senscord::WasmMemory*>(channel_memory->area.memory);
  raw_data->address_addr = static_cast<uint32_t>(
      wasm_memory->GetWasmAddress() + channel_memory->area.offset);
  raw_data->size = channel_memory->area.size;
  raw_data->type_addr =
      senscord_context_get_wasm_address(channel_memory->raw_data_type);
  raw_data->timestamp = channel_memory->timestamp;
  return 0;
}

/** senscord_channel_convert_rawdata */
int32_t senscord_channel_convert_rawdata_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t output_rawdata_addr,
    wasm_size_t output_size) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  void* output_rawdata = ToNativePointer<void*>(inst, output_rawdata_addr);
  return senscord_channel_convert_rawdata(
      channel, output_rawdata, output_size);
}

/** senscord_channel_get_property */
int32_t senscord_channel_get_property_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    const char* property_key,
    wasm_addr_t value_addr,
    wasm_size_t value_size) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  void* value = ToNativePointer<void*>(inst, value_addr);
  return senscord_channel_get_property(
      channel, property_key, value, value_size);
}

/** senscord_channel_get_property_count */
int32_t senscord_channel_get_property_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_channel_get_property_count(channel, count);
}

/** senscord_channel_get_property_key */
int32_t senscord_channel_get_property_key_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    uint32_t index,
    wasm_addr_t property_key_addr) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_channel_get_property_key() is not supported."));
  return -1;
}

/** senscord_channel_get_property_key_string */
int32_t senscord_channel_get_property_key_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    uint32_t index,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_channel_get_property_key_string(
      channel, index, buffer, length);
}

/** senscord_channel_get_updated_property_count */
int32_t senscord_channel_get_updated_property_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_channel_get_updated_property_count(channel, count);
}

/** senscord_channel_get_updated_property_key */
int32_t senscord_channel_get_updated_property_key_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    uint32_t index,
    wasm_addr_t property_key_addr) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_channel_get_updated_property_key() is not supported."));
  return -1;
}

/** senscord_channel_get_updated_property_key_string */
int32_t senscord_channel_get_updated_property_key_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_channel_t channel,
    uint32_t index,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_channel_get_updated_property_key_string(
      channel, index, buffer, length);
}

/* =============================================================
 * Environment APIs
 * ============================================================= */

/** senscord_set_file_search_path */
int32_t senscord_set_file_search_path_wrapper(
    wasm_exec_env_t exec_env,
    const char* paths) {
  return senscord_set_file_search_path(paths);
}

/** senscord_get_file_search_path */
int32_t senscord_get_file_search_path_wrapper(
    wasm_exec_env_t exec_env,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_get_file_search_path(buffer, length);
}

/* =============================================================
 * Configuration APIs
 * ============================================================= */

/** senscord_config_create */
int32_t senscord_config_create_wrapper(
    wasm_exec_env_t exec_env,
    wasm_addr_t config_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_config_t* config =
      ToNativePointer<senscord_config_t*>(inst, config_addr);
  int32_t ret = senscord_config_create(config);
  if (ret == 0) {
    senscord_context_set_config(exec_env, *config, SENSCORD_CONTEXT_OP_ENTER);
  }
  return ret;
}

/** senscord_config_destroy */
int32_t senscord_config_destroy_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config) {
  int32_t ret = senscord_config_destroy(config);
  if (ret == 0) {
    senscord_context_set_config(exec_env, config, SENSCORD_CONTEXT_OP_EXIT);
  }
  return ret;
}

/** senscord_config_add_stream */
int32_t senscord_config_add_stream_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* stream_key,
    const char* instance_name,
    const char* stream_type,
    int32_t port_id) {
  return senscord_config_add_stream(
      config, stream_key, instance_name, stream_type, port_id);
}

/** senscord_config_set_stream_buffering */
int32_t senscord_config_set_stream_buffering_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* stream_key,
    enum senscord_buffering_t buffering,
    int32_t num,
    enum senscord_buffering_format_t format) {
  return senscord_config_set_stream_buffering(
      config, stream_key, buffering, num, format);
}

/** senscord_config_add_stream_argument */
int32_t senscord_config_add_stream_argument_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* stream_key,
    const char* argument_name,
    const char* argument_value) {
  return senscord_config_add_stream_argument(
      config, stream_key, argument_name, argument_value);
}

/** senscord_config_add_instance */
int32_t senscord_config_add_instance_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* instance_name,
    const char* component_name) {
  return senscord_config_add_instance(
      config, instance_name, component_name);
}

/** senscord_config_add_instance_argument */
int32_t senscord_config_add_instance_argument_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* instance_name,
    const char* argument_name,
    const char* argument_value) {
  return senscord_config_add_instance_argument(
      config, instance_name, argument_name, argument_value);
}

/** senscord_config_add_instance_allocator */
int32_t senscord_config_add_instance_allocator_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* instance_name,
    const char* allocator_key,
    const char* allocator_name) {
  return senscord_config_add_instance_allocator(
      config, instance_name, allocator_key, allocator_name);
}

/** senscord_config_add_allocator */
int32_t senscord_config_add_allocator_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* allocator_key,
    const char* type,
    int32_t cacheable) {
  return senscord_config_add_allocator(
      config, allocator_key, type, cacheable);
}

/** senscord_config_add_allocator_argument */
int32_t senscord_config_add_allocator_argument_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* allocator_key,
    const char* argument_name,
    const char* argument_value) {
  return senscord_config_add_allocator_argument(
      config, allocator_key, argument_name, argument_value);
}

/** senscord_config_add_converter */
int32_t senscord_config_add_converter_wrapper(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    const char* converter_name,
    int32_t enable_property,
    int32_t enable_rawdata) {
  return senscord_config_add_converter(
      config, converter_name, enable_property, enable_rawdata);
}

/* =============================================================
 * Utility APIs
 * ============================================================= */

/** senscord_property_key_set_channel_id */
int32_t senscord_property_key_set_channel_id_wrapper(
    wasm_exec_env_t exec_env,
    const char* key,
    uint32_t channel_id,
    wasm_addr_t made_key_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* made_key = ToNativePointer<char*>(inst, made_key_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_property_key_set_channel_id(
      key, channel_id, made_key, length);
}

/* =============================================================
 * Event argument APIs
 * ============================================================= */

/** senscord_event_argument_getvalue_int8 */
int32_t senscord_event_argument_getvalue_int8_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  int8_t* value = ToNativePointer<int8_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_int8(args, key, value);
}

/** senscord_event_argument_getvalue_int16 */
int32_t senscord_event_argument_getvalue_int16_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  int16_t* value = ToNativePointer<int16_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_int16(args, key, value);
}

/** senscord_event_argument_getvalue_int32 */
int32_t senscord_event_argument_getvalue_int32_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  int32_t* value = ToNativePointer<int32_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_int32(args, key, value);
}

/** senscord_event_argument_getvalue_int64 */
int32_t senscord_event_argument_getvalue_int64_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  int64_t* value = ToNativePointer<int64_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_int64(args, key, value);
}

/** senscord_event_argument_getvalue_uint8 */
int32_t senscord_event_argument_getvalue_uint8_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint8_t* value = ToNativePointer<uint8_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_uint8(args, key, value);
}

/** senscord_event_argument_getvalue_uint16 */
int32_t senscord_event_argument_getvalue_uint16_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint16_t* value = ToNativePointer<uint16_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_uint16(args, key, value);
}

/** senscord_event_argument_getvalue_uint32 */
int32_t senscord_event_argument_getvalue_uint32_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* value = ToNativePointer<uint32_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_uint32(args, key, value);
}

/** senscord_event_argument_getvalue_uint64 */
int32_t senscord_event_argument_getvalue_uint64_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint64_t* value = ToNativePointer<uint64_t*>(inst, value_addr);
  return senscord_event_argument_getvalue_uint64(args, key, value);
}

/** senscord_event_argument_getvalue_float */
int32_t senscord_event_argument_getvalue_float_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  float* value = ToNativePointer<float*>(inst, value_addr);
  return senscord_event_argument_getvalue_float(args, key, value);
}

/** senscord_event_argument_getvalue_double */
int32_t senscord_event_argument_getvalue_double_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t value_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  double* value = ToNativePointer<double*>(inst, value_addr);
  return senscord_event_argument_getvalue_double(args, key, value);
}

/** senscord_event_argument_getvalue_string */
int32_t senscord_event_argument_getvalue_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_event_argument_getvalue_string(args, key, buffer, length);
}

/** senscord_event_argument_getvalue_binary */
int32_t senscord_event_argument_getvalue_binary_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  void* buffer = ToNativePointer<void*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_event_argument_getvalue_binary(args, key, buffer, length);
}

/** senscord_event_argument_get_serialized_binary */
int32_t senscord_event_argument_get_serialized_binary_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, const char* key,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  void* buffer = ToNativePointer<void*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_event_argument_get_serialized_binary(
      args, key, buffer, length);
}

/** senscord_event_argument_get_element_count */
int32_t senscord_event_argument_get_element_count_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args,
    wasm_addr_t count_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  uint32_t* count = ToNativePointer<uint32_t*>(inst, count_addr);
  return senscord_event_argument_get_element_count(args, count);
}

/** senscord_event_argument_get_key_string */
int32_t senscord_event_argument_get_key_string_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, uint32_t index,
    wasm_addr_t buffer_addr,
    wasm_addr_t length_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  char* buffer = ToNativePointer<char*>(inst, buffer_addr);
  uint32_t* length = ToNativePointer<uint32_t*>(inst, length_addr);
  return senscord_event_argument_get_key_string(
      args, index, buffer, length);
}

/** senscord_event_argument_get_key */
wasm_addr_t senscord_event_argument_get_key_wrapper(
    wasm_exec_env_t exec_env,
    senscord_event_argument_t args, uint32_t index) {
  c_api::SetLastError(SENSCORD_STATUS_FAIL(
      kBlockName, senscord::Status::kCauseNotSupported,
      "senscord_event_argument_get_key() is not supported."));
  return 0;  // NULL
}

/* ============================================================= */

NativeSymbol kNativeSymbols[] = {
    // Status
    EXPORT_WASM_API_WITH_SIG2(senscord_get_last_error_level, "()i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_get_last_error_cause, "()i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_get_last_error_string, "(iii)i"),

    // Core
    EXPORT_WASM_API_WITH_SIG2(senscord_core_init, "(i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_init_with_config, "(iI)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_exit, "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_get_stream_count, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_get_stream_info, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_get_stream_info_string,
                              "(Iiiii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_get_opened_stream_count, "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_get_version, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_open_stream, "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_open_stream_with_setting,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_core_close_stream, "(II)i"),

    // Stream
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_start, "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_stop, "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_frame, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_release_frame, "(II)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_release_frame_unused, "(II)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_clear_frames, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_property, "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_set_property, "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_userdata_property, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_set_userdata_property, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_property_count, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_property_key, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_get_property_key_string,
                              "(Iiii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_lock_property, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_unlock_property, "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_lock_property_with_key,
                              "(Iiiii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_unlock_property_by_resource,
                              "(II)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_register_frame_callback,
                              "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_unregister_frame_callback,
                              "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_register_event_callback,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_register_event_callback2,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_stream_unregister_event_callback,
                              "(I$)i"),

    // Frame
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_sequence_number, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_type, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_channel_count, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_channel, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_channel_from_channel_id,
                              "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_frame_get_user_data, "(Ii)i"),

    // Channel
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_channel_id, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_raw_data, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_raw_data_handle, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_convert_rawdata, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_property, "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_property_count, "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_property_key, "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_property_key_string,
                              "(Iiii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_updated_property_count,
                              "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_updated_property_key,
                              "(Iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_channel_get_updated_property_key_string,
                              "(Iiii)i"),

    // Environment
    EXPORT_WASM_API_WITH_SIG2(senscord_set_file_search_path, "($)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_get_file_search_path, "(ii)i"),

    // Config
    EXPORT_WASM_API_WITH_SIG2(senscord_config_create, "(i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_destroy, "(I)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_stream, "(I$$$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_set_stream_buffering,
                              "(I$iii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_stream_argument, "(I$$$)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_instance, "(I$$)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_instance_argument,
                              "(I$$$)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_instance_allocator,
                              "(I$$$)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_allocator, "(I$$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_allocator_argument,
                              "(I$$$)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_config_add_converter, "(I$ii)i"),

    // Utils
    EXPORT_WASM_API_WITH_SIG2(senscord_property_key_set_channel_id, "($iii)i"),

    // EventArgument
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_int8, "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_int16,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_int32,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_int64,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_uint8,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_uint16,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_uint32,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_uint64,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_float,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_double,
                              "(I$i)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_string,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_getvalue_binary,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_get_serialized_binary,
                              "(I$ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_get_element_count,
                              "(Ii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_get_key_string,
                              "(Iiii)i"),
    EXPORT_WASM_API_WITH_SIG2(senscord_event_argument_get_key, "(Ii)i"),
};

char kModuleName[] = "env";

}  // namespace

/**
 * @brief Returns native symbols.
 * @param[out] module_name    Module name.
 * @param[out] native_symbols Native symbols.
 * @return Number of native symbols.
 */
extern "C" uint32_t get_native_lib(
    char** module_name, NativeSymbol** native_symbols) {
  *module_name = kModuleName;
  *native_symbols = kNativeSymbols;
  return sizeof(kNativeSymbols) / sizeof(NativeSymbol);
}
