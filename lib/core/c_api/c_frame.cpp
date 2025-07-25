/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <vector>
#include <string>
#include <iterator>

#include "senscord/c_api/senscord_c_api.h"
#include "senscord/senscord.h"
#include "senscord/frame.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "frame/frame_core.h"
#include "c_api/c_common.h"
#include "util/autolock.h"

namespace c_api = senscord::c_api;
namespace util = senscord::util;
namespace osal = senscord::osal;

/**
 * @brief Get the sequential number of frame.
 * @param[in]  frame         Frame handle.
 * @param[out] frame_number  The number of this frame.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetSequenceNumber
 */
int32_t senscord_frame_get_sequence_number(
    senscord_frame_t frame,
    uint64_t* frame_number) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(frame_number == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  *frame_number = frame_ptr->GetFrameInfo().sequence_number;
  return 0;
}

/**
 * @brief Get type of frame.
 * @param[in]  frame  Frame handle.
 * @param[out] type   Type of frame.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetType
 */
int32_t senscord_frame_get_type(
    senscord_frame_t frame,
    const char** type) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(type == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  *type = frame_ptr->GetType().c_str();
  return 0;
}

/**
 * @brief Get channel count.
 * @param[in]  frame          Frame handle.
 * @param[out] channel_count  Location of channel count.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetChannelList
 */
int32_t senscord_frame_get_channel_count(
    senscord_frame_t frame,
    uint32_t* channel_count) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(channel_count == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  const senscord::ChannelList& channel_list = frame_ptr->GetChannelList();
  *channel_count = static_cast<uint32_t>(channel_list.size());
  return 0;
}

/**
 * @brief Get channel data.
 * @param[in]  frame    Frame handle.
 * @param[in]  index    Index of channel list. (min=0, max=count-1)
 * @param[out] channel  Channel handle.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetChannelList
 */
int32_t senscord_frame_get_channel(
    senscord_frame_t frame,
    uint32_t index,
    senscord_channel_t* channel) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(channel == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  const senscord::ChannelList& channel_list = frame_ptr->GetChannelList();
  if (index >= channel_list.size()) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "index(%" PRIu32 ") is larger than list.size(%" PRIuS ")",
        index, channel_list.size()));
    return -1;
  }
  senscord::ChannelList::const_iterator itr = channel_list.begin();
  std::advance(itr, index);
  *channel = c_api::ToHandle(itr->second);
  return 0;
}

/**
 * @brief Get channel data from channel id.
 * @param[in]  frame       Frame handle.
 * @param[in]  channel_id  Channel ID to get.
 * @param[out] channel     Channel handle.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetChannel
 */
int32_t senscord_frame_get_channel_from_channel_id(
    senscord_frame_t frame,
    uint32_t channel_id,
    senscord_channel_t* channel) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(channel == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  senscord::Channel* channel_ptr = NULL;
  senscord::Status status = frame_ptr->GetChannel(channel_id, &channel_ptr);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  *channel = c_api::ToHandle(channel_ptr);
  return 0;
}

/**
 * @brief Get the user data.
 * @param[in]  frame      Frame handle.
 * @param[out] user_data  User data.
 * @return 0 is success or minus is failed (error code).
 * @see Frame::GetUserData
 */
int32_t senscord_frame_get_user_data(
    senscord_frame_t frame,
    struct senscord_user_data_t* user_data) {
  SENSCORD_C_API_ARGUMENT_CHECK(frame == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(user_data == NULL);
  senscord::FrameCore* frame_ptr =
      c_api::ToPointer<senscord::FrameCore*>(frame);
  senscord::Frame::UserData tmp_user_data = {};
  senscord::Status status = frame_ptr->GetUserData(&tmp_user_data);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  user_data->address = tmp_user_data.address;
  user_data->size = tmp_user_data.size;
  return 0;
}
