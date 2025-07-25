/*
 * SPDX-FileCopyrightText: 2022-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string>

#include "senscord/senscord.h"
#include "senscord/osal.h"
#include "c_api/c_common.h"
#include "senscord/c_api/senscord_c_api.h"

namespace c_api = senscord::c_api;

/**
 * @brief Set the channel id to property key.
 * @param[in] key  Property key.
 * @param[in] channel_id  Channel ID
 * @param[out] made_key  Property key + Channel ID
 * @param[in,out] length  [in] made_key buffer size.
 *                        [out] made_key length.
 * @return 0 is success or minus is failed.
 */
int32_t senscord_property_key_set_channel_id(
    const char* key, uint32_t channel_id,
    char* made_key, uint32_t* length) {
  SENSCORD_C_API_ARGUMENT_CHECK(key == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(length == NULL);

  std::string key_make =
      senscord::PropertyUtils::SetChannelId(key, channel_id);
  if (key_make.empty()) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        senscord::kStatusBlockCore, senscord::Status::kCauseInvalidArgument,
        "key format error. key=%s", key));
    *length = 0;
    return -1;
  }

  senscord::Status status = c_api::StringToCharArray(
      key_make, made_key, length);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  return 0;
}
