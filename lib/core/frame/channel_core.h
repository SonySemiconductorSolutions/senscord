/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CORE_FRAME_CHANNEL_CORE_H_
#define LIB_CORE_FRAME_CHANNEL_CORE_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include "senscord/frame.h"
#include "senscord/develop/common_types.h"
#include "stream/stream_core.h"
#include "stream/property_history_book.h"
#include "util/mutex.h"
#include "util/resource_list.h"

namespace senscord {

/**
 * @brief Channel core of Frame.
 */
class ChannelCore : public Channel {
 public:
  /**
   * @brief Constructor
   * @param[in] (channel_raw_data) the all parameter of this channel.
   * @param[in] (parent_stream) Parent stream address.
   * @param[in] (history_book) PropertyHistoryBook related to this channel.
   */
  explicit ChannelCore(
      const ChannelRawData& channel_raw_data,
      const StreamCore* parent_stream,
      PropertyHistoryBook* history_book);

  /**
   * @brief Destructor
   */
  virtual ~ChannelCore();

  /**
   * @brief  Get the channel ID.
   * @param[out] (channel_id) the channel ID.
   * @return Status object.
   */
  virtual Status GetChannelId(uint32_t* channel_id) const;

  /**
   * @brief  Get the raw data information.
   * @param[out] (raw_data) the raw data information.
   * @return Status object.
   */
  virtual Status GetRawData(Channel::RawData* raw_data) const;

  /**
   * @brief Get the stored property key list on this channel.
   * @param (key_list) Stored property key list.
   * @return Status object.
   */
  virtual Status GetPropertyList(
    std::vector<std::string>* key_list) const;

  /**
   * @brief Get the updated property key list on this channel.
   * @param[out] (key_list) Updated property key list.
   * @return Status object.
   */
  virtual Status GetUpdatedPropertyList(
    std::vector<std::string>* key_list) const;

#ifdef SENSCORD_SERIALIZE
  /**
   * @brief Get the serialized property related to this raw data.
   * @param (property_key) Key of property to get.
   * @param (property) Location of serialized property.
   * @param (property_size) Size of serialized property.
   * @return Status object.
   */
  virtual Status GetSerializedProperty(
    const std::string& property_key,
    void** property,
    size_t* property_size) const;
#else
  /**
   * @brief Get the property related to this raw data.
   * @param[in] (property_key) Key of property to get.
   * @param[out] (property) Location of property.
   * @return Status object.
   */
  virtual Status GetProperty(
      const std::string& property_key, void* property) const;
#endif  // SENSCORD_SERIALIZE

  /**
   * @brief Get the raw data with memory use informations.
   * @param[out] (memory) Raw data with memory use.
   * @return Status object.
   */
  virtual Status GetRawDataMemory(MemoryContained* memory) const;

  /**
   * @brief  Get the channel ID.
   * @return Channel ID.
   */
  uint32_t GetChannelId() const {
    return channel_raw_data_.channel_id;
  }

  /**
   * @brief Get the type of raw data. (for C API)
   * return Type of raw data.
   */
  const std::string& GetType() const {
    return channel_raw_data_.data_type;
  }

  /**
   * @brief Get the stored property key list on this channel.
   *        (for C API)
   * @return Stored property key list.
   */
  const std::map<std::string, uint32_t>& GetPropertyList() const {
    return properties_;
  }

  /**
   * @brief Get the updated property key list on this channel.
   * @return Updated property key list.
   */
  const std::vector<std::string>& GetUpdatedPropertyList() const;

  /**
   * @brief Gets the resource list.
   */
  ResourceList* GetResources() { return &resources_; }

 private:
  const ChannelRawData& channel_raw_data_;
  const StreamCore* parent_stream_;

  PropertyHistoryBook* history_book_;
  std::map<std::string, uint32_t> properties_;

  ResourceList resources_;
};

}  // namespace senscord

#endif  // LIB_CORE_FRAME_CHANNEL_CORE_H_
