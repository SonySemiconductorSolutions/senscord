/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/pseudo_image_source.h"
#include <string>
#include <vector>
#include <utility>
#include "senscord/senscord.h"

// blockname for status
static const char kBlockName[] = "PseudoImage";

// Default values
static const uint32_t kDefaultFrameRateNum = 60;
static const uint32_t kDefaultWidth = 200;
static const uint32_t kDefaultHeight = 200;

// Channel Info
static const uint32_t kChannelMax = 2;

/**
 * @brief Check to different from two properties.
 */
static bool IsDifferent(
    const senscord::ImageProperty& a, const senscord::ImageProperty& b) {
  if ((a.height != b.height) || (a.width != b.width) ||
      (a.stride_bytes != b.stride_bytes)) {
    return true;
  }
  return false;
}

/**
 * @brief Check to different from two properties.
 */
static bool IsDifferent(const PseudoImageProperty& a,
    const PseudoImageProperty& b) {
  if ((a.x != b.x) || (a.y != b.y) || (a.z != b.z)) {
    return true;
  }
  return false;
}

/**
 * @brief Get the round-up value.
 */
static uint32_t RoundUp(uint32_t value, uint32_t step) {
  return ((value + (step - 1)) / step) * step;
}

/**
 * @brief Open the stream source.
 * @param[in] (core) The core instance.
 * @param[in] (util) The utility accessor to core.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Open(
    senscord::Core* core,
    senscord::StreamSourceUtility* util) {
  SENSCORD_LOG_DEBUG("[pseudo] open");
  util_ = util;

  // get allocator (use default)
  senscord::Status status = util_->GetAllocator(
      senscord::kAllocatorNameDefault, &allocator_);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // register optional properties.
  SENSCORD_REGISTER_PROPERTY(util,
      kPseudoImagePropertyKey, PseudoImageProperty);

  // parse arguments
  // width, height
  {
    uint64_t value = 0;
    status = util_->GetStreamArgument("width", &value);
    if (status.ok()) {
      image_property_.width = static_cast<uint32_t>(value);
      image_property_.stride_bytes = RoundUp(static_cast<uint32_t>(value), 16);
    }
    SENSCORD_LOG_INFO("[pseudo] width = %" PRIu32, image_property_.width);

    status = util_->GetStreamArgument("height", &value);
    if (status.ok()) {
      image_property_.height = static_cast<uint32_t>(value);
    }
    SENSCORD_LOG_INFO("[pseudo] height = %" PRIu32, image_property_.height);
  }

  // framerate
  {
    uint64_t fps = 0;
    status = util_->GetStreamArgument("fps", &fps);
    if (status.ok() && fps > 0) {
      senscord::FrameRateProperty framerate = {};
      framerate.num = static_cast<uint32_t>(fps);
      framerate.denom = 1;
      Set(senscord::kFrameRatePropertyKey, &framerate);
    }
    SENSCORD_LOG_INFO("[pseudo] framerate = %" PRIu32 " / %" PRIu32,
        framerate_.num, framerate_.denom);
  }

  // set channel property
  for (uint32_t index = 0; index < kChannelMax; ++index) {
    util_->UpdateChannelProperty(senscord::kChannelIdImage(index),
        senscord::kImagePropertyKey, &image_property_);
  }

  return senscord::Status::OK();
}

/**
 * @brief Close the stream source.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Close() {
  SENSCORD_LOG_DEBUG("[pseudo] close");
  return senscord::Status::OK();
}

/**
 * @brief Pull up the new frames.
 * @param[out] (frames) The information about new frames.
 */
void PseudoImageSource::GetFrames(std::vector<senscord::FrameInfo>* frames) {
  // wait to finish to create the new raw data.
  senscord::osal::OSSleep(sleep_nsec_);

  // create information.
  std::string rawdata_type = senscord::kRawDataTypeImage;
  size_t raw_size = image_property_.height * image_property_.stride_bytes;
  uint64_t timestamp = 0;
  senscord::osal::OSGetTime(&timestamp);

    // setup frame.
  senscord::FrameInfo frameinfo = {};
  frameinfo.sequence_number = frame_seq_num_++;

  for (uint32_t index = 0; index < kChannelMax; ++index) {
    senscord::Memory* raw_memory = NULL;
    if (raw_size > 0) {
      // allocate the new memory.
      senscord::Status status = allocator_->Allocate(raw_size, &raw_memory);
      if (!status.ok()) {
        SENSCORD_LOG_WARNING("allocation failed!: seq_num=%" PRIu64,
            frameinfo.sequence_number);
        util_->SendEventFrameDropped(frameinfo.sequence_number);
        ReleaseFrame(frameinfo, NULL);
        return;
      }

      // copy the rawdata to allocated memory.
      senscord::osal::OSMemset(
          reinterpret_cast<uint8_t*>(raw_memory->GetAddress()),
          static_cast<uint8_t>(frameinfo.sequence_number & 0xFF),
          raw_size);
    }

    // initialize channel informations.
    senscord::ChannelRawData channel = {};
    channel.channel_id = senscord::kChannelIdImage(index);
    channel.data_type = rawdata_type;
    channel.data_memory = raw_memory;
    channel.data_size = raw_size;
    channel.data_offset = 0;
    channel.captured_timestamp = timestamp;

    frameinfo.channels.push_back(channel);
  }

  frames->push_back(frameinfo);
}

/**
 * @brief Release the used frame.
 * @param[in] (frameinfo) The information about used frame.
 * @param[in] (referenced_channel_ids) List of referenced channel IDs.
 *                                     (NULL is the same as empty)
 * @return The status of function.
 */
senscord::Status PseudoImageSource::ReleaseFrame(
    const senscord::FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids) {
  // free raw data
  typedef std::vector<senscord::ChannelRawData> ChannelRawList;
  ChannelRawList::const_iterator itr = frameinfo.channels.begin();
  ChannelRawList::const_iterator end = frameinfo.channels.end();
  for (; itr != end; ++itr) {
    if (itr->data_memory != NULL) {
      allocator_->Free(itr->data_memory);
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Get the stream source property.
 * @param[in] (key) The key of property.
 * @param[out] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Get(
    const std::string& key, senscord::ChannelInfoProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  for (uint32_t index = 0; index < kChannelMax; ++index) {
    senscord::ChannelInfo info = {};
    info.raw_data_type = senscord::kRawDataTypeImage;
    info.description = "Sample image raw data";

    property->channels.insert(
        std::make_pair(senscord::kChannelIdImage(index), info));
  }
  return senscord::Status::OK();
}

/**
 * @brief Get the stream source property.
 * @param[in] (key) The key of property.
 * @param[out] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Get(
    const std::string& key, senscord::FrameRateProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  *property = framerate_;
  return senscord::Status::OK();
}

/**
 * @brief Set the new stream source property.
 * @param[in] (key) The key of property.
 * @param[in] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Set(
    const std::string& key, const senscord::FrameRateProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  if (property->denom == 0 || property->num == 0) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "0 value");
  }

  uint64_t new_sleep_nsec =
      ((1000ULL * 1000 * 1000) * property->denom) / property->num;

  if (sleep_nsec_ != new_sleep_nsec) {
    framerate_ = *property;
    sleep_nsec_ = new_sleep_nsec;

    SENSCORD_LOG_INFO("change framerate to %" PRId32 " / %" PRId32,
        property->num, property->denom);

    // notify to streams
    util_->SendEventPropertyUpdated(key);
  }
  return senscord::Status::OK();
}

/**
 * @brief Get the stream source property.
 * @param[in] (key) The key of property.
 * @param[out] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Get(
    const std::string& key, senscord::ImageProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  *property = image_property_;
  return senscord::Status::OK();
}

/**
 * @brief Set the new stream source property.
 * @param[in] (key) The key of property.
 * @param[in] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Set(
    const std::string& key, const senscord::ImageProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }

  if (image_property_.pixel_format != property->pixel_format) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument,
        "Changing pixel format is not supported");
  }

  if (IsDifferent(image_property_, *property)) {
    image_property_ = *property;
    image_property_.stride_bytes = RoundUp(image_property_.width, 16);

    // notify to channel
    for (uint32_t index = 0; index < kChannelMax; ++index) {
      util_->UpdateChannelProperty(senscord::kChannelIdImage(index),
          senscord::kImagePropertyKey, &image_property_);
    }

    // notify to streams
    util_->SendEventPropertyUpdated(key);
  }
  return senscord::Status::OK();
}

/**
 * @brief Get the stream source property.
 * @param[in] (key) The key of property.
 * @param[out] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Get(
    const std::string& key,
    senscord::ImageSensorFunctionSupportedProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  property->auto_exposure_supported = false;
  property->auto_white_balance_supported = false;
  property->brightness_supported = false;
  property->iso_sensitivity_supported = false;
  property->exposure_time_supported = false;
  property->exposure_metering_supported = false;
  property->gamma_value_supported = false;
  property->gain_value_supported = false;
  property->hue_supported = false;
  property->saturation_supported = false;
  property->sharpness_supported = false;
  property->white_balance_supported = false;
  return senscord::Status::OK();
}

/**
 * @brief Get the stream source property.
 * @param[in] (key) The key of property.
 * @param[out] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Get(
    const std::string& key, PseudoImageProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }
  *property = pseudo_image_;
  return senscord::Status::OK();
}

/**
 * @brief Set the new stream source property.
 * @param[in] (key) The key of property.
 * @param[in] (property) The location of property.
 * @return The status of function.
 */
senscord::Status PseudoImageSource::Set(
    const std::string& key, const PseudoImageProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kBlockName,
        senscord::Status::kCauseInvalidArgument, "Null pointer");
  }

  if (IsDifferent(pseudo_image_, *property)) {
    pseudo_image_ = *property;

    // notify to streams
    util_->SendEventPropertyUpdated(key);
  }
  return senscord::Status::OK();
}

/**
 * @brief Constructor
 * @param[in] (allocator) Memory allocator for this source.
 */
PseudoImageSource::PseudoImageSource()
    : util_(), allocator_(), frame_seq_num_(0) {
  SENSCORD_LOG_DEBUG("[pseudo] constructor");

  // setup properties to default value.
  framerate_.num   = kDefaultFrameRateNum;
  framerate_.denom = 1;
  sleep_nsec_ = (1000ULL * 1000 * 1000) / kDefaultFrameRateNum;

  image_property_.width  = kDefaultWidth;
  image_property_.height = kDefaultHeight;
  image_property_.stride_bytes = RoundUp(image_property_.width, 16);
  image_property_.pixel_format = senscord::kPixelFormatGREY;

  pseudo_image_.x = 100;
  pseudo_image_.y = 200;
  pseudo_image_.z = "hoge";
}

/**
 * @brief Destructor
 */
PseudoImageSource::~PseudoImageSource() {
  util_ = NULL;
  allocator_ = NULL;
}
