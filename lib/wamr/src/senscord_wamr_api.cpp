/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <pthread.h>

#include <sstream>

#include "senscord/status.h"
#include "senscord/logger.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/c_api/senscord_c_api.h"
#include "c_api/c_common.h"
#include "stream/stream_core.h"
#include "frame/frame_core.h"
#include "frame/channel_core.h"
#include "util/resource_list.h"
#include "util/mutex.h"
#include "util/autolock.h"
#include "wasm_allocator/src/wasm_memory_allocator.h"
#include "src/senscord_wamr_types.h"
#include "src/senscord_wamr_context.h"

#include "wasm_export.h"

// TODO: Remove this include (wasm_cluster_is_thread_terminated)
#include "../libraries/thread-mgr/thread_manager.h"

/**
 * @brief Initializes the SensCord native library.
 */
extern "C" int init_native_lib(void) {
  return senscord_context_init();
}

/**
 * @brief Exits the SensCord native library.
 */
extern "C" void deinit_native_lib(void) {
  senscord_context_exit();
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

/**
 * @brief WASM heap.
 */
class WasmHeap : public senscord::ResourceData {
 public:
  WasmHeap() : inst_(), wasm_address_() {}

  ~WasmHeap() {
    Free();
  }

  /**
   * @brief Duplicate native data to WASM heap.
   * @param[in] inst  WASM module instance.
   * @param[in] buffer  Native data.
   * @param[in] size  Size of native data.
   */
  senscord::Status DuplicateData(
      wasm_module_inst_t inst, const void* buffer, uint32_t size) {
    SENSCORD_STATUS_ARGUMENT_CHECK(buffer == NULL);
    if (wasm_address_ != 0) {
      return SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseInvalidOperation,
          "already allocated memory.");
    }
    wasm_addr_t address = wasm_runtime_module_dup_data(
        inst, reinterpret_cast<const char*>(buffer), size);
    if (address == 0) {
      return SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseResourceExhausted,
          "wasm_runtime_module_dup_data() failed. size=%" PRIu32, size);
    }
    inst_ = inst;
    wasm_address_ = address;
    return senscord::Status::OK();
  }

  /**
   * @brief Free the WASM heap.
   */
  void Free() {
    if (wasm_address_ != 0) {
      wasm_runtime_module_free(inst_, wasm_address_);
      wasm_address_ = 0;
    }
  }

  /**
   * @brief Get the WASM address.
   */
  wasm_addr_t GetWasmAddress() const {
    return wasm_address_;
  }

 private:
  wasm_module_inst_t inst_;
  wasm_addr_t wasm_address_;
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
 * @brief Initialize core.
 */
int32_t InitCore(
    wasm_exec_env_t exec_env,
    wasm_addr_t core_addr,
    senscord_config_t config) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_core_t* core = ToNativePointer<senscord_core_t*>(inst, core_addr);
  int32_t ret = 0;

  // Add a default allocator to config object
  ret = senscord_config_add_allocator(
      config, SENSCORD_CONFIG_DEFAULT_ALLOCATOR_KEY,
      kAllocatorTypeWasm, 0);
  if (ret == 0) {
    std::ostringstream buf;
    buf << reinterpret_cast<uint64_t>(exec_env);
    senscord_config_add_allocator_argument(
        config, SENSCORD_CONFIG_DEFAULT_ALLOCATOR_KEY,
        senscord::kArgumentWasmExecEnv, buf.str().c_str());
  }

  ret = senscord_core_init_with_config(core, config);
  if (ret == 0) {
    ret = senscord_context_set_core(
        exec_env, *core, SENSCORD_CONTEXT_OP_ENTER);
    if (ret != 0) {
      senscord_core_exit(*core);
      c_api::SetLastError(SENSCORD_STATUS_FAIL(
          kBlockName, senscord::Status::kCauseResourceExhausted,
          "wasm_runtime_spawn_thread() failed."));
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

/** senscord_core_open_stream */
int32_t senscord_core_open_stream_wrapper(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    const char* stream_key,
    wasm_addr_t stream_addr) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_stream_t* stream =
      ToNativePointer<senscord_stream_t*>(inst, stream_addr);
  int32_t ret = senscord_core_open_stream(core, stream_key, stream);
  if (ret == 0) {
    senscord_context_set_stream(
        exec_env, *stream, core, SENSCORD_CONTEXT_OP_ENTER);
  }
  return ret;
}

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
    senscord_context_set_stream(
        exec_env, *stream, core, SENSCORD_CONTEXT_OP_ENTER);
  }
  return ret;
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

/** senscord_stream_start */
int32_t senscord_stream_start_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return senscord_stream_start(stream);
}

/** senscord_stream_stop */
int32_t senscord_stream_stop_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  WasmBlockingOperation blocking_op(exec_env);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  return senscord_stream_stop(stream);
}

/** senscord_stream_get_frame */
int32_t senscord_stream_get_frame_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    wasm_addr_t frame_addr,
    int32_t timeout_msec) {
  WasmBlockingOperation blocking_op(exec_env, stream);
  if (!blocking_op.GetResult()) {
    return -1;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_frame_t* frame =
      ToNativePointer<senscord_frame_t*>(inst, frame_addr);
  return senscord_stream_get_frame(stream, frame, timeout_msec);
}


/** senscord_stream_release_frame */
int32_t senscord_stream_release_frame_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    senscord_frame_t frame) {
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
  senscord::util::Mutex mutex;
  WasmFrameCallbackParam* param;

  WasmFrameCallback() : mutex(), param() {
  }

  ~WasmFrameCallback() {
    delete param;
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
    bool thread_env_inited = false;
    if (!wasm_runtime_thread_env_inited()) {
      thread_env_inited = wasm_runtime_init_thread_env();
    }

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

    if (thread_env_inited) {
      wasm_runtime_destroy_thread_env();
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
  SENSCORD_C_API_ARGUMENT_CHECK(stream == 0);
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
    senscord::util::AutoLock _lock(&frame_callback->mutex);
    delete frame_callback->param;
    frame_callback->param = param;
  }

  return 0;
}

/** senscord_stream_unregister_frame_callback */
int32_t senscord_stream_unregister_frame_callback_wrapper(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream) {
  SENSCORD_C_API_ARGUMENT_CHECK(stream == 0);
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
  senscord::util::Mutex mutex;
  WasmEventCallbackList list;

  WasmEventCallback() : mutex(), list() {
  }

  ~WasmEventCallback() {
    for (WasmEventCallbackList::const_iterator
        itr = list.begin(), end = list.end(); itr != end; ++itr) {
      delete itr->second;
    }
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
    bool thread_env_inited = false;
    if (!wasm_runtime_thread_env_inited()) {
      thread_env_inited = wasm_runtime_init_thread_env();
    }

    bool ret = false;
    {
      wasm_module_inst_t inst = wasm_runtime_get_module_inst(param->exec_env);
      WasmHeap type_heap;
      type_heap.DuplicateData(
          inst, event_type.c_str(), event_type.size() + 1);

      if (param->callback_addr != 0) {
        senscord_stream_t stream_handle = c_api::ToHandle(stream);
        senscord_event_argument_t event_handle = c_api::ToHandle(&args);
        uint32_t argv[6];
        // argv[0]-[1]: 64bit stream handle
        senscord::osal::OSMemcpy(
            &argv[0], sizeof(uint32_t) * 2,
            &stream_handle, sizeof(senscord_stream_t));
        argv[2] = type_heap.GetWasmAddress();
        // argv[3]-[4]: 64bit event argument handle
        senscord::osal::OSMemcpy(
            &argv[3], sizeof(uint32_t) * 2,
            &event_handle, sizeof(senscord_event_argument_t));
        argv[5] = param->private_data;
        ret = wasm_runtime_call_indirect(
            param->exec_env, param->callback_addr, 6, argv);
      } else if (param->callback_old_addr != 0) {
        uint32_t argv[3];
        argv[0] = type_heap.GetWasmAddress();
        argv[1] = 0;  // reserved
        argv[2] = param->private_data;
        ret = wasm_runtime_call_indirect(
            param->exec_env, param->callback_old_addr, 3, argv);
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

    if (thread_env_inited) {
      wasm_runtime_destroy_thread_env();
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
    senscord::util::AutoLock _lock(&event_callback->mutex);
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
    senscord::util::AutoLock _lock(&event_callback->mutex);
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
    const char* kResourceFrameType = "wasm_frame_type";
    const std::string& frame_type = frame_ptr->GetType();
    uint32_t type_size = static_cast<uint32_t>(frame_type.size() + 1);
    WasmHeap* heap =
        frame_ptr->GetResources()->Create<WasmHeap>(kResourceFrameType);
    if ((heap->GetWasmAddress() == 0) && (type_size != 0)) {
      senscord::Status status =
          heap->DuplicateData(inst, frame_type.c_str(), type_size);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    *type_addr = heap->GetWasmAddress();
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

/** senscord_frame_get_channel */
int32_t senscord_frame_get_channel_wrapper(
    wasm_exec_env_t exec_env,
    senscord_frame_t frame,
    uint32_t index,
    wasm_addr_t channel_addr) {
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  senscord_channel_t* channel =
      ToNativePointer<senscord_channel_t*>(inst, channel_addr);
  return senscord_frame_get_channel(frame, index, channel);
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
  return senscord_frame_get_channel_from_channel_id(
      frame, channel_id, channel);
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

  {
    const char* kResourceUserData = "wasm_user_data";
    WasmHeap* heap =
        frame_ptr->GetResources()->Create<WasmHeap>(kResourceUserData);
    if ((heap->GetWasmAddress() == 0) && (tmp_user_data.size != 0)) {
      senscord::Status status =
          heap->DuplicateData(inst, tmp_user_data.address, tmp_user_data.size);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    user_data->address_addr = heap->GetWasmAddress();
    user_data->size = static_cast<wasm_size_t>(tmp_user_data.size);
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
    WasmHeap* heap =
        channel_ptr->GetResources()->Create<WasmHeap>(kResourceRawDataType);
    if ((heap->GetWasmAddress() == 0) && (type_size != 0)) {
      senscord::Status status =
          heap->DuplicateData(inst, tmp_raw_data.type.c_str(), type_size);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    uintptr_t wasm_address = static_cast<uintptr_t>(heap->GetWasmAddress());
    raw_data->type = reinterpret_cast<char*>(wasm_address);
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
  senscord::Channel::RawData tmp_raw_data = {};
  {
    senscord::Status status = channel_ptr->GetRawData(&tmp_raw_data);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
  }

  senscord::MemoryContained memory = {};
  channel_ptr->GetRawDataMemory(&memory);
  std::string allocator_type;
  if (memory.memory != NULL) {
    allocator_type = memory.memory->GetAllocator()->GetType();
  }

  if (allocator_type == kAllocatorTypeWasm) {
    senscord::WasmMemory* wasm_memory =
        reinterpret_cast<senscord::WasmMemory*>(memory.memory);
    raw_data->address_addr =
        static_cast<uint32_t>(wasm_memory->GetWasmAddress() + memory.offset);
    raw_data->size = static_cast<uint32_t>(tmp_raw_data.size);
    raw_data->type_addr = 0;
    raw_data->timestamp = tmp_raw_data.timestamp;
  } else {
    // duplicate rawdata
    const char* kResourceRawData = "wasm_raw_data";
    WasmHeap* heap =
        channel_ptr->GetResources()->Create<WasmHeap>(kResourceRawData);
    if ((heap->GetWasmAddress() == 0) && (tmp_raw_data.size != 0)) {
      senscord::Status status =
          heap->DuplicateData(inst, tmp_raw_data.address, tmp_raw_data.size);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    raw_data->address_addr = heap->GetWasmAddress();
    raw_data->size = static_cast<wasm_size_t>(tmp_raw_data.size);
    raw_data->type_addr = 0;
    raw_data->timestamp = tmp_raw_data.timestamp;
  }
  {
    // raw_data_type
    const char* kResourceRawDataType = "wasm_raw_data_type";
    uint32_t type_size = static_cast<uint32_t>(tmp_raw_data.type.size() + 1);
    WasmHeap* heap =
        channel_ptr->GetResources()->Create<WasmHeap>(kResourceRawDataType);
    if ((heap->GetWasmAddress() == 0) && (type_size != 0)) {
      senscord::Status status =
          heap->DuplicateData(inst, tmp_raw_data.type.c_str(), type_size);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    raw_data->type_addr = heap->GetWasmAddress();
  }
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
