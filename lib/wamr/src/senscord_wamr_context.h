/*
 * SPDX-FileCopyrightText: 2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_WAMR_SRC_SENSCORD_WAMR_CONTEXT_H_
#define LIB_WAMR_SRC_SENSCORD_WAMR_CONTEXT_H_

#include <stdint.h>

#include "senscord/c_api/senscord_c_api.h"

#include "wasm_export.h"

/**
 * @brief Operation type for context.
 */
enum senscord_context_op_t {
  SENSCORD_CONTEXT_OP_ENTER,
  SENSCORD_CONTEXT_OP_EXIT,
};

/**
 * @brief Initializes the senscord context.
 * @return 0 for success, -1 for failure.
 */
int32_t senscord_context_init();

/**
 * @brief Exits the senscord context.
 */
void senscord_context_exit();

/**
 * @brief Sets the config handle to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] config      Config handle.
 * @param[in] operation   Operation type.
 */
void senscord_context_set_config(
    wasm_exec_env_t exec_env,
    senscord_config_t config,
    enum senscord_context_op_t operation);

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
    enum senscord_context_op_t operation);

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
    enum senscord_context_op_t operation);

/**
 * @brief Sets the blocking stream to context.
 * @param[in] exec_env    WASM execution environment.
 * @param[in] stream      Stream handle.
 * @param[in] operation   Operation type.
 */
void senscord_context_set_blocking_stream(
    wasm_exec_env_t exec_env,
    senscord_stream_t stream,
    enum senscord_context_op_t operation);

#endif  // LIB_WAMR_SRC_SENSCORD_WAMR_CONTEXT_H_
