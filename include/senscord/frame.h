/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_FRAME_H_
#define SENSCORD_FRAME_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include "senscord/config.h"
#include "senscord/noncopyable.h"
#include "senscord/status.h"
#include "senscord/memory.h"
#include "senscord/senscord_types.h"

namespace senscord {

/**
 * @brief Channel of frame interface class.
 */
class Channel : private util::Noncopyable {
 public:
  /**
   * @brief Raw data informations.
   */
  typedef senscord::RawData RawData;

  /**
   * @brief  Get the channel ID.
   * @param[out] (channel_id) the channel ID.
   * @return Status object.
   */
  virtual Status GetChannelId(uint32_t* channel_id) const = 0;

  /**
   * @brief  Get the raw data information.
   * @param[out] (raw_data) the raw data information.
   * @return Status object.
   */
  virtual Status GetRawData(Channel::RawData* raw_data) const = 0;

  /**
   * @brief Get the property related to this raw data.
   * @param[in] (property_key) Key of property to get.
   * @param[out] (property) Location of property.
   * @return Status object.
   */
#ifdef SENSCORD_SERIALIZE
  template <typename T>
  Status GetProperty(const std::string& property_key, T* property) const;
#else
  virtual Status GetProperty(
      const std::string& property_key, void* property) const = 0;
#endif  // SENSCORD_SERIALIZE

  /**
   * @brief Get the stored property key list on this channel.
   * @param[out] (key_list) Stored property key list.
   * @return Status object.
   */
  virtual Status GetPropertyList(
    std::vector<std::string>* key_list) const = 0;

  /**
   * @brief Get the updated property key list on this channel.
   * @param[out] (key_list) Updated property key list.
   * @return Status object.
   */
  virtual Status GetUpdatedPropertyList(
    std::vector<std::string>* key_list) const = 0;

  /**
   * @brief Get the raw data with memory use informations.
   * @param[out] (memory) Raw data with memory use.
   * @return Status object.
   */
  virtual Status GetRawDataMemory(MemoryContained* memory) const = 0;

  /**
   * @brief Virtual destructor.
   */
  virtual ~Channel() {}

#ifdef SENSCORD_SERIALIZE
 protected:
  /**
   * @brief Get the serialized property related to this raw data.
   * @param[in] (property_key) Key of property to get.
   * @param[out] (property) Location of serialized property.
   * @param[out] (property_size) Size of serialized property.
   * @return Status object.
   */
  virtual Status GetSerializedProperty(
    const std::string& property_key,
    void** property,
    size_t* property_size) const = 0;
#endif  // SENSCORD_SERIALIZE
};

/**
 * @brief List of channels with channel id as key.
 */
typedef std::map<uint32_t, Channel*> ChannelList;

/**
 * @brief Frame interface class.
 */
class Frame : private util::Noncopyable {
 public:
  /**
   * @brief Raw data informations.
   */
  struct UserData {
    void* address;      /**< virtual address */
    size_t size;        /**< data size */
  };

  /**
   * @brief Get the sequential number of frame.
   * @param[out] (sequence_number) The number of this frame.
   * @return Status object.
   */
  virtual Status GetSequenceNumber(uint64_t* sequence_number) const = 0;

  /**
   * @brief Get the time of frame was sent. Based on GetTime() of OSAL.
   * @param[out] (sent_time) Time when frame was sent.
   * @return Status object.
   */
  virtual Status GetSentTime(uint64_t* sent_time) const = 0;

  /**
   * @brief Get type.
   * @param[out] (type) Type of this frame.
   * @return Status object.
   */
  virtual Status GetType(std::string* type) const = 0;

  /**
   * @brief Get channel list.
   * @param[out] (list) Location of all channel list.
   * @return Status object.
   */
  virtual Status GetChannelList(ChannelList* list) const = 0;

  /**
   * @brief Get channel data.
   * @param[in] (channel_id) Channel ID to get.
   * @param[out] (channel) Channel object.
   * @return Status object.
   */
  virtual Status GetChannel(
    uint32_t channel_id,
    Channel** channel) const = 0;

  /**
   * @brief  Get the user data.
   * @param[out] (user_data) the user data.
   * @return Status object.
   */
  virtual Status GetUserData(Frame::UserData* user_data) const = 0;

  /**
   * @brief Virtual destructor.
   */
  virtual ~Frame() {}
};

}   // namespace senscord

#ifdef SENSCORD_SERIALIZE
// implementations of template methods.
#include "senscord/frame_private.h"
#endif  // SENSCORD_SERIALIZE
#endif  // SENSCORD_FRAME_H_
