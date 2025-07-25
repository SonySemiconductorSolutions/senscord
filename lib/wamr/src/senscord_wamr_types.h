/*
 * SPDX-FileCopyrightText: 2023 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_WAMR_SRC_SENSCORD_WAMR_TYPES_H_
#define LIB_WAMR_SRC_SENSCORD_WAMR_TYPES_H_

#include <stdint.h>

// __wasm32__
typedef uint32_t wasm_addr_t;
typedef uint32_t wasm_size_t;

struct senscord_user_data_wasm_t {
  wasm_addr_t address_addr;
  wasm_size_t size;
};

struct senscord_raw_data_wasm_t {
  wasm_addr_t address_addr;
  wasm_size_t size;
  wasm_addr_t type_addr;
  uint64_t timestamp;
};

struct senscord_raw_data_handle_t {
  uint64_t address;   /**< virtual address */
  uint64_t size;      /**< data size */
  const char *type;   /**< data type*/
  uint64_t timestamp; /**< nanoseconds timestamp captured by the device */
};
#endif  // LIB_WAMR_SRC_SENSCORD_WAMR_TYPES_H_
