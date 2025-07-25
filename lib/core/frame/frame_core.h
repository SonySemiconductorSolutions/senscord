/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CORE_FRAME_FRAME_CORE_H_
#define LIB_CORE_FRAME_FRAME_CORE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "senscord/frame.h"
#include "stream/stream_core.h"
#include "core/internal_types.h"
#include "frame/channel_core.h"
#include "util/resource_list.h"

#include "senscord/develop/extension.h"

namespace senscord {

/**
 * @brief Frame core.
 */
class FrameCore : public Frame {
 public:
  /**
   * @brief Constructor
   * @param[in] (stream) Parent stream.
   * @param[in] (info) Frame information.
   * @param[in] (sent_time) Time when this frame was sent.
   */
  explicit FrameCore(
      const StreamCore* stream, const FrameInfo& info,
      uint64_t sent_time);

  /**
   * @brief Destructor
   */
  virtual ~FrameCore();

  /**
   * @brief Get the sequential number of frame.
   * @param[out] (sequence_number) The number of this frame.
   * @return Status object.
   */
  virtual Status GetSequenceNumber(uint64_t* sequence_number) const;

  /**
   * @brief Get the time of frame was sent. Based on GetTime() of OSAL.
   * @param[out] (sent_time) Time when frame was sent.
   * @return Status object.
   */
  virtual Status GetSentTime(uint64_t* sent_time) const;

  /**
   * @brief Get type.
   * @param[out] (type) Type of this frame.
   * @return Status object.
   */
  virtual Status GetType(std::string* type) const;

  /**
   * @brief Get channel list.
   * @param[out] (list) Location of all channel list.
   * @return Status object.
   */
  virtual Status GetChannelList(ChannelList* list) const;

  /**
   * @brief Get channel data.
   * @param[in] (channel_id) Channel ID to get.
   * @param[out] (channel) Channel object.
   * @return Status object.
   */
  virtual Status GetChannel(uint32_t channel_id, Channel** channel) const;

  /**
   * @brief  Get the user data.
   * @param[out] (user_data) the user data.
   * @return Status object.
   */
  virtual Status GetUserData(Frame::UserData* user_data) const;

  /**
   * @brief Set user data.
   * @param[in] (user_data) User data of this frame.
   */
  void SetUserData(const FrameUserData& user_data);

  /**
   * @brief Set the masking channels.
   * @param[in] the list of channel IDs.
   */
  void SetChannelMask(const std::vector<uint32_t>& mask);

  /**
   * @brief Disable channel mask.
   * @param[in] (disabled) Channel mask disable.
   */
  void SetDisableChannelMask(bool disabled);

  /**
   * @brief Check whether channel is masked.
   * @param[in] (channel_id) channel id.
   * @return true means channel is masked.
   */
  bool IsMaskedChannel(uint32_t channel_id) const;

  /**
   * @brief Get frame information.
   * @return Frame information.
   */
  const FrameInfo& GetFrameInfo() const { return frame_info_; }

  /**
   * @brief Get type. (for C API)
   * @return Type of this frame.
   */
  const std::string& GetType() const { return parent_stream_->GetType(); }

  /**
   * @brief Get channel list.
   */
  const ChannelList& GetChannelList() const;

#ifdef SENSCORD_RECORDER
  /**
   * @brief Set the flag of recorded.
   */
  void NotifyRecorded() { is_recorded_ = true; }

  /**
   * @brief Get the flag of recorded.
   * @return True means accessed by recorder.
   */
  bool IsRecorded() const { return is_recorded_; }
#endif  // SENSCORD_RECORDER

  /**
   * @brief Set the extension frame info.
   * @param[in] (frameinfo) Extension frame info
   * @param[in] (history_book) Extension PropertyHistoryBook.
   */
  void SetExtensionFrameInfo(
      ExtensionFrameInfo* frameinfo,
      PropertyHistoryBook* history_book);

  /**
   * @brief Get the extension frame info.
   * @return Extension frame info.
   */
  ExtensionFrameInfo* GetExtensionFrameInfo();

  /**
   * @brief Gets the resource list.
   */
  ResourceList* GetResources() { return &resources_; }

 private:
  // parent stream
  const StreamCore* parent_stream_;

  // frame information
  FrameInfo frame_info_;

  // channel list
  std::vector<ChannelCore*> channel_list_;

  // resource list
  ResourceList resources_;

  // user data
  std::vector<uint8_t> user_data_;

  // the list of masked channel IDs
  std::vector<uint32_t> masked_channels_;

  // disable channel mask
  bool mask_disabled_;

  // extension frame info
  ExtensionFrameInfo* extension_frame_info_;

#ifdef SENSCORD_RECORDER
  // referenced by recorder
  bool is_recorded_;
#endif  // SENSCORD_RECORDER
};

}  // namespace senscord

#endif  // LIB_CORE_FRAME_FRAME_CORE_H_
