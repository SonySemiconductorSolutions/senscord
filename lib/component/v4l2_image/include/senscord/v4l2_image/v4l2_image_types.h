/*
 * SPDX-FileCopyrightText: 2018-2022 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_V4L2_IMAGE_V4L2_IMAGE_TYPES_H_
#define SENSCORD_V4L2_IMAGE_V4L2_IMAGE_TYPES_H_

#include <stdint.h>
#include <string>
#include "senscord/serialize.h"

/**
 * Sample Property Key for V4L2 Image.
 */
const char kV4L2SamplePropertyKey[] = "v4l2_sample_property";

/**
 * @brief Sample Property for V4L2 Image.
 */
struct V4L2SampleProperty {
  int32_t a;        /**< sample member a */
  uint32_t b;       /**< sample member b */
  std::string c;    /**< sample member c */

  SENSCORD_SERIALIZE_DEFINE(a, b, c)
};

#endif  // SENSCORD_V4L2_IMAGE_V4L2_IMAGE_TYPES_H_
