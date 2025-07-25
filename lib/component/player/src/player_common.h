/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PLAYER_SRC_PLAYER_COMMON_H_
#define LIB_COMPONENT_PLAYER_SRC_PLAYER_COMMON_H_

#include <map>
#include <string>
#include <vector>

#include "senscord/property_types.h"
#include "senscord/develop/recorder_common.h"
#include "senscord/develop/common_types.h"

// Key is a property key and Value is an binary data of property
typedef std::map<std::string, senscord::BinaryProperty> BinaryPropertyList;

/**
 * @brief Playback Frame
 */
struct PlayFrame {
  senscord::FrameInfo frame_info;
  uint32_t index;  // for PlayPositionProperty
  std::map<uint32_t, BinaryPropertyList> properties;
};

// The struct for info.xml
/**
 * @brief info xml stream information
 */
struct InfoXmlStreamInfo {
  std::string key;
  std::string type;
  uint32_t frame_num;
  uint32_t frame_denom;
  uint32_t skip_frame;
  std::vector<std::string> property_keys;
};

/**
 * @brief info xml channel parameter
 */
struct InfoXmlChannelParameter {
  std::string rawdata_type;
  std::string description;
  bool mask;
};
// Key=Channel-ID
typedef std::map<uint32_t, InfoXmlChannelParameter> InfoXmlChannelList;

/**
 * @brief info xml parameter
 */
struct InfoXmlParameter {
  std::string record_date;
  InfoXmlStreamInfo stream;
  InfoXmlChannelList channels;
};

/**
 * @brief record raw data
 */
struct RecordRawData {
  senscord::RecordDataType record_type;
  uint64_t captured_timestamp;
  std::string rawdata_type;
};
typedef std::map<uint32_t, RecordRawData> RecordChannelData;

/**
 * @brief record frame data
 */
struct RecordFrameData {
  uint64_t sequence_number;
  uint64_t sent_time;
  // key=channel_id, value=raw or composite_raw
  RecordChannelData channels;
};

#endif  // LIB_COMPONENT_PLAYER_SRC_PLAYER_COMMON_H_
