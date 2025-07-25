/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frame/channel_core.h"

#include <algorithm>
#include "stream/stream_core.h"
#include "util/autolock.h"

namespace {

/**
 * @brief Resource for updated property list.
 */
const char kResourceUpdatedPropertyList[] = "updated_property_list";

struct ResourceUpdatedPropertyList : public senscord::ResourceData {
  bool build;
  std::vector<std::string> property_list;
};

}  // namespace

namespace senscord {

/**
 * @brief Constructor
 * @param[in] (channel_raw_data) the all parameter of this channel.
 * @param[in] (parent_stream) Parent stream address.
 * @param[in] (history_book) PropertyHistoryBook related to this channel.
 */
ChannelCore::ChannelCore(
    const ChannelRawData& channel_raw_data, const StreamCore* parent_stream,
    PropertyHistoryBook* history_book)
    : channel_raw_data_(channel_raw_data), parent_stream_(parent_stream),
      history_book_(history_book) {
  if (history_book_ != NULL) {
    history_book_->ReferenceCurrentProperties(
        channel_raw_data_.channel_id, &properties_);
  }

  resources_.Create<ResourceUpdatedPropertyList>(kResourceUpdatedPropertyList);
}

/**
 * @brief Destructor
 */
ChannelCore::~ChannelCore() {
  if (history_book_ != NULL) {
    history_book_->ReleaseProperties(
        channel_raw_data_.channel_id, properties_);
    history_book_ = NULL;
  }
}

/**
 * @brief  Get the channel ID.
 * @param[out] (channel_id) the channel ID.
 * @return Status object.
 */
Status ChannelCore::GetChannelId(uint32_t* channel_id) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(channel_id == NULL);
  *channel_id = channel_raw_data_.channel_id;
  return Status::OK();
}

/**
 * @brief  Get the raw data information.
 * @param[out] (raw_data) the raw data information.
 * @return Status object.
 */
Status ChannelCore::GetRawData(Channel::RawData* raw_data) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(raw_data == NULL);
  if (channel_raw_data_.data_memory) {
    raw_data->address = reinterpret_cast<void*>(
        channel_raw_data_.data_memory->GetAddress() +
        channel_raw_data_.data_offset);
    raw_data->size = channel_raw_data_.data_size;
  } else {
    raw_data->address = NULL;
    raw_data->size = 0;
  }
  raw_data->type = channel_raw_data_.data_type;
  raw_data->timestamp = channel_raw_data_.captured_timestamp;
  return Status::OK();
}

/**
 * @brief Get the stored property key list on this channel.
 * @param (key_list) Stored property key list.
 * @return Status object.
 */
Status ChannelCore::GetPropertyList(
    std::vector<std::string>* key_list) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(key_list == NULL);
  std::vector<std::string> tmp_list;
  for (std::map<std::string, uint32_t>::const_iterator
      itr = properties_.begin(), end = properties_.end(); itr != end; ++itr) {
    tmp_list.push_back(itr->first);
  }
  key_list->swap(tmp_list);
  return Status::OK();
}

/**
 * @brief Get the updated property key list on this channel.
 * @param[out] (key_list) Updated property key list.
 * @return Status object.
 */
Status ChannelCore::GetUpdatedPropertyList(
    std::vector<std::string>* key_list) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(key_list == NULL);
  *key_list = GetUpdatedPropertyList();
  return Status::OK();
}

/**
 * @brief Get the updated property key list on this channel.
 * @return Updated property key list.
 */
const std::vector<std::string>& ChannelCore::GetUpdatedPropertyList() const {
  util::AutoLock lock(parent_stream_->GetFrameMutex());
  ResourceUpdatedPropertyList* resource =
      resources_.Get<ResourceUpdatedPropertyList>(
          kResourceUpdatedPropertyList);
  if (history_book_ != NULL) {
    if (!resource->build) {
      resource->build = true;
      history_book_->GetUpdatedPropertyList(
          parent_stream_, channel_raw_data_.channel_id, properties_,
          &resource->property_list);
    }
  }
  return resource->property_list;
}

#ifdef SENSCORD_SERIALIZE
/**
 * @brief Get the serialized property related to this raw data.
 * @param (property_key) Key of property to get.
 * @param (property) Location of serialized property.
 * @param (property_size) Size of serialized property.
 * @return Status object.
 */
Status ChannelCore::GetSerializedProperty(
    const std::string& property_key,
    void** property,
    size_t* property_size) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(property == NULL);
  SENSCORD_STATUS_ARGUMENT_CHECK(property_size == NULL);
  if (history_book_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseUnknown, "property history book is not set");
  }

  std::string extract_property_key =
      senscord::PropertyUtils::GetKey(property_key);
  std::map<std::string, uint32_t>::const_iterator itr =
      properties_.find(extract_property_key);
  if (itr == properties_.end()) {
    // not stored key.
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseNotFound, "property not found: key=%s",
        property_key.c_str());
  }
  Status status = history_book_->GetProperty(channel_raw_data_.channel_id,
      extract_property_key, itr->second, property, property_size);
  return SENSCORD_STATUS_TRACE(status);
}
#else
/**
 * @brief Get the property related to this raw data.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Location of property.
 * @return Status object.
 */
Status ChannelCore::GetProperty(
    const std::string& property_key, void* property) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(property == NULL);
  if (history_book_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseUnknown, "property history book is not set");
  }

  std::string extract_property_key =
      senscord::PropertyUtils::GetKey(property_key);
  std::map<std::string, uint32_t>::const_iterator itr =
      properties_.find(extract_property_key);
  if (itr == properties_.end()) {
    // not stored key.
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseNotFound, "property not found: key=%s",
        property_key.c_str());
  }
  Status status = history_book_->GetProperty(
      channel_raw_data_.channel_id,
      extract_property_key, itr->second, property);
  return SENSCORD_STATUS_TRACE(status);
}
#endif  // SENSCORD_SERIALIZE

/**
 * @brief Get the raw data with memory use informations.
 * @param[out] (memory) Raw data with memory use.
 * @return Status object.
 */
Status ChannelCore::GetRawDataMemory(MemoryContained* memory) const {
  SENSCORD_STATUS_ARGUMENT_CHECK(memory == NULL);
  memory->memory = channel_raw_data_.data_memory;
  memory->size = channel_raw_data_.data_size;
  memory->offset = channel_raw_data_.data_offset;
  return Status::OK();
}

}  // namespace senscord
