/*
 * SPDX-FileCopyrightText: 2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/senscord_wamr_context.h"

#include <inttypes.h>
#include <signal.h>

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "senscord/osal.h"
#include "senscord/logger.h"
#include "util/mutex.h"
#include "util/autolock.h"

#if 0
#include <stdio.h>
#define LOG_E(fmt, ...) fprintf(stderr, (fmt "\n"), ##__VA_ARGS__)
#define LOG_W(fmt, ...) fprintf(stderr, (fmt "\n"), ##__VA_ARGS__)
#define LOG_I(fmt, ...) fprintf(stderr, (fmt "\n"), ##__VA_ARGS__)
#define LOG_D(fmt, ...) fprintf(stderr, (fmt "\n"), ##__VA_ARGS__)
#else
#define LOG_TAG "wasm"
#define LOG_E(...) SENSCORD_LOG_ERROR_TAGGED(LOG_TAG, __VA_ARGS__)
#define LOG_W(...) SENSCORD_LOG_WARNING_TAGGED(LOG_TAG, __VA_ARGS__)
#define LOG_I(...) SENSCORD_LOG_INFO_TAGGED(LOG_TAG, __VA_ARGS__)
#define LOG_D(...) SENSCORD_LOG_DEBUG_TAGGED(LOG_TAG, __VA_ARGS__)
#endif

namespace {

const int kSignalNumber = SIGUSR1;
const uint64_t kInterruptInterval = 500000000;  // 500ms

struct CoreParam {
  std::set<senscord_stream_t> streams;
};

/**
 * @brief SensCord context.
 */
struct SensCordContext {
  wasm_thread_t thread;
  senscord::util::Mutex mutex;
  senscord::osal::OSCond* cond;
  std::set<senscord_config_t> config_handles;
  std::map<senscord_core_t, CoreParam> core_handles;
  std::set<senscord_stream_t> blocking_stream_handles;

  SensCordContext()
      : thread(), mutex(), cond() {
    senscord::osal::OSCreateCond(&cond);
  }

  ~SensCordContext() {
    senscord::osal::OSDestroyCond(cond);
    cond = NULL;
  }
};

static void* g_context_key = NULL;
static senscord::osal::OSMutex* g_mutex = NULL;
static bool g_signal_setup = false;

static volatile sig_atomic_t g_interrupt_flag = 0;
static struct sigaction g_prev_sigaction;

/**
 * @brief Signal handler for interrupt.
 */
void senscord_wamr_sigaction(int sig, siginfo_t* siginfo, void* sig_context) {
  LOG_I("senscord_wamr_sigaction: %d", sig);
  g_interrupt_flag = 1;

  if (g_prev_sigaction.sa_flags & SA_SIGINFO) {
    g_prev_sigaction.sa_sigaction(sig, siginfo, sig_context);
  } else if (g_prev_sigaction.sa_handler != NULL) {
    g_prev_sigaction.sa_handler(sig);
  }
}

/**
 * @brief Closes the stream.
 */
void senscord_core_close_stream_force(
    const std::pair<senscord_core_t, senscord_stream_t>& itr) {
  LOG_D("senscord_core_close_stream(force): core=%" PRIx64 ", stream=%" PRIx64,
        itr.first, itr.second);
  senscord_core_close_stream(itr.first, itr.second);
}

/**
 * @brief Extracts the blocking streams.
 */
void senscord_context_extract_blocking_streams(
    SensCordContext* context,
    std::vector<std::pair<senscord_core_t, senscord_stream_t> >* streams) {
  for (std::set<senscord_stream_t>::const_iterator
      itr = context->blocking_stream_handles.begin(),
      end = context->blocking_stream_handles.end();
      itr != end; ++itr) {
    senscord_stream_t stream = *itr;
    for (std::map<senscord_core_t, CoreParam>::iterator
        itr2 = context->core_handles.begin(),
        end2 = context->core_handles.end();
        itr2 != end2; ++itr2) {
      senscord_core_t core = itr2->first;
      if (itr2->second.streams.erase(stream) > 0) {
        streams->push_back(std::make_pair(core, stream));
      }
    }
  }
  context->blocking_stream_handles.clear();
}

/**
 * @brief Context thread.
 */
void* senscord_context_thread(
    wasm_exec_env_t exec_env, void* args) {
  LOG_D("senscord_context_thread <S>");
  SensCordContext* context = reinterpret_cast<SensCordContext*>(args);
  senscord::util::AutoLock _lock(&context->mutex);

  while (true) {
    senscord::osal::OSRelativeTimedWaitCond(
        context->cond, context->mutex.GetObject(), kInterruptInterval);
    if (context->thread == 0) {
      break;
    }
    if (g_interrupt_flag) {
      g_interrupt_flag = 0;
      LOG_D("senscord_context_thread: interrupt");
      std::vector<std::pair<senscord_core_t, senscord_stream_t> > streams;
      senscord_context_extract_blocking_streams(context, &streams);
      context->mutex.Unlock();
      std::for_each(
          streams.begin(), streams.end(),
          senscord_core_close_stream_force);
      context->mutex.Lock();
    }
  }

  LOG_D("senscord_context_thread <E>");
  return 0;
}

/**
 * @brief Creates the context thread.
 */
int32_t senscord_context_create_thread(
    wasm_exec_env_t exec_env, SensCordContext* context) {
  int32_t ret = wasm_runtime_spawn_thread(
      exec_env,
      &context->thread,
      senscord_context_thread,
      context);
  LOG_D("senscord_context_create_thread: ret=%" PRId32 ", tid=%" PRIxPTR,
        ret, context->thread);
  return ret;
}

/**
 * @brief Joins the context thread.
 */
void senscord_context_join_thread(SensCordContext* context) {
  wasm_thread_t thread = 0;
  if (context != NULL) {
    senscord::util::AutoLock _lock(&context->mutex);
    thread = context->thread;
    context->thread = 0;
    senscord::osal::OSSignalCond(context->cond);
  }
  if (thread != 0) {
    LOG_D("senscord_context_join_thread: tid=%" PRIxPTR, thread);
    wasm_runtime_join_thread(thread, NULL);
  }
}

/**
 * @brief Destroys the config handle.
 */
void senscord_config_destroy_force(senscord_config_t config) {
  LOG_D("senscord_config_destroy(force): config=%" PRIx64, config);
  senscord_config_destroy(config);
}

/**
 * @brief Exits the core handle.
 */
void senscord_core_exit_force(
    const std::pair<senscord_core_t, CoreParam>& itr) {
  LOG_D("senscord_core_exit(force): core=%" PRIx64, itr.first);
  senscord_core_exit(itr.first);
}

/**
 * @brief Destroys the senscord context.
 */
void senscord_context_destroy(
    wasm_module_inst_t module_inst,
    void* context) {
  if (context != NULL) {
    LOG_D("senscord_context_destroy");
    SensCordContext* sc_context = reinterpret_cast<SensCordContext*>(context);
    {
      senscord::util::AutoLock _lock(&sc_context->mutex);
      // force release config handles
      std::for_each(
          sc_context->config_handles.begin(),
          sc_context->config_handles.end(),
          senscord_config_destroy_force);
      sc_context->config_handles.clear();
      // force release core handles
      std::for_each(
          sc_context->core_handles.begin(),
          sc_context->core_handles.end(),
          senscord_core_exit_force);
      sc_context->core_handles.clear();
    }
    senscord_context_join_thread(sc_context);
    delete sc_context;
  }
}

/**
 * @brief Gets the senscord context.
 */
SensCordContext* senscord_context_get_instance(
    wasm_module_inst_t module_inst) {
  SensCordContext* context = NULL;
  if (g_context_key != NULL) {
    context = reinterpret_cast<SensCordContext*>(
        wasm_runtime_get_context(module_inst, g_context_key));
    if (context == NULL) {
      senscord::osal::OSLockMutex(g_mutex);
      context = reinterpret_cast<SensCordContext*>(
          wasm_runtime_get_context(module_inst, g_context_key));
      if (context == NULL) {
        context = new SensCordContext();
        wasm_runtime_set_context_spread(module_inst, g_context_key, context);
      }
      senscord::osal::OSUnlockMutex(g_mutex);
    }
  }
  return context;
}

}  // namespace

/**
 * @brief Initializes the senscord context.
 * @return 0 for success, -1 for failure.
 */
int32_t senscord_context_init() {
  LOG_D("senscord_context_init");
  if (g_context_key != NULL) {
    LOG_W("senscord_context_init: already initialized");
    return 0;
  }
  // context key
  g_context_key = wasm_runtime_create_context_key(senscord_context_destroy);
  if (g_context_key == NULL) {
    LOG_E("senscord_context_init: wasm_runtime_create_context_key failed");
    return -1;
  }
  // mutex
  senscord::osal::OSCreateMutex(&g_mutex);
  // signal (setup)
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = senscord_wamr_sigaction;
  int32_t ret = sigaction(kSignalNumber, &sa, &g_prev_sigaction);
  if (ret != 0) {
    LOG_E("senscord_context_init: sigaction failed: %d", errno);
    senscord_context_exit();
    return -1;
  }
  g_signal_setup = true;
  return 0;
}

/**
 * @brief Exits the senscord context.
 */
void senscord_context_exit() {
  LOG_D("senscord_context_exit");
  // signal (restore)
  if (g_signal_setup) {
    g_signal_setup = false;
    sigaction(kSignalNumber, &g_prev_sigaction, NULL);
  }
  // mutex
  senscord::osal::OSDestroyMutex(g_mutex);
  g_mutex = NULL;
  // context key
  wasm_runtime_destroy_context_key(g_context_key);
  g_context_key = NULL;
}

/**
 * @brief Sets the config handle to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] config      Config handle.
 * @param[in] operation   Operation type.
 */
void senscord_context_set_config(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    enum senscord_context_op_t operation) {
  wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
  SensCordContext* context = senscord_context_get_instance(module_inst);
  if (context != NULL) {
    senscord::util::AutoLock _lock(&context->mutex);
    if (operation == SENSCORD_CONTEXT_OP_ENTER) {
      LOG_D("senscord_context_set_config: add: config=%" PRIx64, config);
      context->config_handles.insert(config);
    } else if (operation == SENSCORD_CONTEXT_OP_EXIT) {
      LOG_D("senscord_context_set_config: remove: config=%" PRIx64, config);
      context->config_handles.erase(config);
    }
  }
}

/**
 * @brief Sets the core handle to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] core        Core handle.
 * @param[in] operation   Operation type.
 * @return 0 for success, -1 for failure.
 */
int32_t senscord_context_set_core(
    wasm_exec_env_t exec_env,
    senscord_core_t core,
    enum senscord_context_op_t operation) {
  wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
  SensCordContext* context = senscord_context_get_instance(module_inst);
  if (context != NULL) {
    if (operation == SENSCORD_CONTEXT_OP_ENTER) {
      LOG_D("senscord_context_set_core: add: core=%" PRIx64, core);
      senscord::util::AutoLock _lock(&context->mutex);
      context->core_handles[core] = CoreParam();
      // create thread
      if (context->thread == 0) {
        int32_t ret = senscord_context_create_thread(exec_env, context);
        if (ret != 0) {
          LOG_E("senscord_context_set_core: create thread failed");
          context->core_handles.erase(core);
          return -1;
        }
      }
    } else if (operation == SENSCORD_CONTEXT_OP_EXIT) {
      LOG_D("senscord_context_set_core: remove: core=%" PRIx64, core);
      bool empty = false;
      {
        senscord::util::AutoLock _lock(&context->mutex);
        std::map<senscord_core_t, CoreParam>::iterator itr =
            context->core_handles.find(core);
        if (itr != context->core_handles.end()) {
          std::set<senscord_stream_t> result;
          std::set_difference(
              context->blocking_stream_handles.begin(),
              context->blocking_stream_handles.end(),
              itr->second.streams.begin(),
              itr->second.streams.end(),
              std::inserter(result, result.end()));
          context->blocking_stream_handles.swap(result);
          context->core_handles.erase(itr);
        }
        empty = context->core_handles.empty();
      }
      if (empty) {
        senscord_context_join_thread(context);
      }
    }
  }
  return 0;
}

/**
 * @brief Sets the stream handle to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] stream      Stream handle.
 * @param[in] parent_core Parent core handle.
 * @param[in] operation   Operation type.
 */
void senscord_context_set_stream(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    senscord_core_t parent_core,
    enum senscord_context_op_t operation) {
  wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
  SensCordContext* context = senscord_context_get_instance(module_inst);
  if (context != NULL) {
    senscord::util::AutoLock _lock(&context->mutex);
    std::map<senscord_core_t, CoreParam>::iterator itr =
        context->core_handles.find(parent_core);
    if (itr != context->core_handles.end()) {
      if (operation == SENSCORD_CONTEXT_OP_ENTER) {
        LOG_D("senscord_context_set_stream: add: stream=%" PRIx64, stream);
        itr->second.streams.insert(stream);
      } else if (operation == SENSCORD_CONTEXT_OP_EXIT) {
        LOG_D("senscord_context_set_stream: remove: stream=%" PRIx64, stream);
        itr->second.streams.erase(stream);
      }
    }
  }
}

/**
 * @brief Sets the blocking stream to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] stream      Stream handle.
 * @param[in] operation   Operation type.
 */
void senscord_context_set_blocking_stream(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    enum senscord_context_op_t operation) {
  wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
  SensCordContext* context = senscord_context_get_instance(module_inst);
  if (context != NULL) {
    senscord::util::AutoLock _lock(&context->mutex);
    if (operation == SENSCORD_CONTEXT_OP_ENTER) {
      context->blocking_stream_handles.insert(stream);
    } else if (operation == SENSCORD_CONTEXT_OP_EXIT) {
      context->blocking_stream_handles.erase(stream);
    }
  }
}
