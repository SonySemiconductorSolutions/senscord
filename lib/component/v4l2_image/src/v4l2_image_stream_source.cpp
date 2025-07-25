/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/v4l2_image_stream_source.h"
#include <inttypes.h>
#include <stdint.h>
#include <vector>
#include <map>

#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/logger.h"

namespace
{
  const char *kBlockName = "v4l2_image";
  const char *kUseAllocatorName = "image";
  // default values
  const char *kDefaultDevice = "/dev/video0";
  const int32_t kDefaultBufferNum = 6;
  const int32_t kDefaultWidth = 640;
  const int32_t kDefaultHeight = 480;
  const int32_t kDefaultStrideBytes = 640;
  const char *kDefaultPixelFormat = senscord::kPixelFormatNV16;
  const int32_t kDefaultFramerate = 30;

  // spare buffer number
  const int32_t kSpareBufferNum = 2;
} // namespace

/**
 * @brief Constructor.
 */
V4L2ImageStreamSource::V4L2ImageStreamSource()
    : util_(), settings_(), frame_seq_num_(0), allocator_(),
      image_property_(), framerate_property_(), is_started_(),
      is_yuyv_to_nv16_()
{
  settings_.device = kDefaultDevice;
  settings_.buffer_num = kDefaultBufferNum + kSpareBufferNum;

  image_property_.width = kDefaultWidth;
  image_property_.height = kDefaultHeight;
  image_property_.stride_bytes = kDefaultStrideBytes;
  image_property_.pixel_format = kDefaultPixelFormat;

  framerate_property_.num = kDefaultFramerate;
  framerate_property_.denom = 1;

  senscord::osal::OSCreateMutex(&buffer_mutex_);
}

/**
 * @brief Destructor.
 */
V4L2ImageStreamSource::~V4L2ImageStreamSource()
{
  senscord::osal::OSDestroyMutex(buffer_mutex_);
  buffer_mutex_ = NULL;
}

/**
 * @brief Pull up the new frames.
 * @param[out] (frames) The information about new frames.
 */
void V4L2ImageStreamSource::GetFrames(
    std::vector<senscord::FrameInfo> *frames)
{
  struct v4l2_buffer buffer = {};
  senscord::Status status = device_.DequeueBuffer(&buffer);
  if (!status.ok())
  {
    SENSCORD_LOG_DEBUG("dequeue error, next buffer");
    return; // next buffer
  }
  // always have a buffer on the device.
  if (GetUsedBufferNum() >= (settings_.buffer_num - kSpareBufferNum))
  {
    device_.QueueBuffer(&buffer);
    return;
  }

  void *src_address = buffer_list_[buffer.index].addr;
  uint32_t size_frame = buffer.bytesused;
  void *dest_address =
      reinterpret_cast<void *>(
          buffer_list_[buffer.index].memory->GetAddress());
  size_t dest_size = buffer_list_[buffer.index].memory->GetSize();

  if (image_property_.pixel_format == senscord::kPixelFormatNV16 &&
      is_yuyv_to_nv16_)
  {
    /* yuyv to nv16 color format conversion */
    size_frame = (image_property_.width) * (image_property_.height) * 2;
    std::vector<char> dest_nv16_address(size_frame);

    char *byte_element = reinterpret_cast<char *>(src_address);

    for (uint32_t i = 0; i < size_frame; ++i)
    {
      if (i % 2 == 0)
      {
        dest_nv16_address[i / 2] = *byte_element;
      }
      else
      {
        dest_nv16_address[(size_frame + i - 1) / 2] = *byte_element;
      }
      ++byte_element;
    }
    senscord::osal::OSMemcpy(dest_address, dest_size,
                             &dest_nv16_address[0], size_frame);
  }
  else
  {
    // copy without conversion
    senscord::osal::OSMemcpy(dest_address, dest_size,
                             src_address, size_frame);
  }

  senscord::FrameInfo frame = {};
  frame.sequence_number = frame_seq_num_++;

  senscord::ChannelRawData channel_0 = {};
  channel_0.channel_id = 0;
  channel_0.data_type = senscord::kRawDataTypeImage;
  channel_0.data_memory = buffer_list_[buffer.index].memory;
  channel_0.data_size = size_frame;
  channel_0.captured_timestamp = GetNsecTimestamp(buffer.timestamp);

  frame.channels.push_back(channel_0);
  frames->push_back(frame);

  SENSCORD_LOG_DEBUG("GetFrames(t=%" PRIu64 ")",
                     channel_0.captured_timestamp);
}

/**
 * @brief Release the used frame.
 * @param[in] (frameinfo) The information about used frame.
 * @param[in] (referenced_channel_ids) List of referenced channel IDs.
 *                                     (NULL is the same as empty)
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::ReleaseFrame(
    const senscord::FrameInfo &frameinfo,
    const std::vector<uint32_t> *referenced_channel_ids)
{
  if (frameinfo.channels.empty())
  {
    // illegal case
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidArgument, "channel is empty");
  }
  struct v4l2_buffer buffer = {};
  senscord::ChannelRawData channel_0 = frameinfo.channels[0];

  senscord::osal::OSLockMutex(buffer_mutex_);
  for (size_t index = 0; index < buffer_list_.size(); ++index)
  {
    if (buffer_list_[index].memory == channel_0.data_memory)
    {
      SENSCORD_LOG_DEBUG("Release(t=%" PRIu64 ")",
                         channel_0.captured_timestamp);

      buffer_list_[index].used = false;
      device_.QueryBuffer(buffer_list_[index].index, &buffer);
      device_.QueueBuffer(&buffer);
      break;
    }
  }
  senscord::osal::OSUnlockMutex(buffer_mutex_);
  return senscord::Status::OK();
}

/**
 * @brief Open the stream source.
 * @param[in] (core) The core instance.
 * @param[in] (util) The utility accessor to core.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Open(
    senscord::Core *core,
    senscord::StreamSourceUtility *util)
{
  util_ = util;

  // get allocator
  // if there is no specified, use default.
  senscord::Status status = util_->GetAllocator(
      kUseAllocatorName, &allocator_);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok())
  {
    status = util_->GetAllocator(
        senscord::kAllocatorNameDefault, &allocator_);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok())
  {
    // parse xml parameter
    ParseParameter();

    // open device
    status = device_.DevOpen(settings_.device);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Close the stream source.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Close()
{
  senscord::Status status;
  status = device_.DevClose();
  if (!status.ok())
  {
    SENSCORD_LOG_WARNING("%s", status.ToString().c_str());
  }
  return senscord::Status::OK();
}

/**
 * @brief Start the stream source.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Start()
{
  senscord::Status status;

  // setting to device
  status = SetDeviceParameter();
  SENSCORD_STATUS_TRACE(status);

  if (status.ok())
  {
    // allocate memory
    status = AllocateBuffer();
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok())
  {
    // queuing buffer
    for (size_t index = 0; index < buffer_list_.size(); ++index)
    {
      struct v4l2_buffer buffer = {};
      status = device_.QueryBuffer(index, &buffer);
      if (!status.ok())
      {
        SENSCORD_STATUS_TRACE(status);
        break;
      }
      status = device_.QueueBuffer(&buffer);
      if (!status.ok())
      {
        SENSCORD_STATUS_TRACE(status);
        break;
      }
    }
  }

  if (status.ok())
  {
    // start streaming
    status = device_.DevStart();
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok())
  {
    // update frame property
    util_->UpdateChannelProperty(0, senscord::kImagePropertyKey,
                                 &image_property_);
    is_started_ = true;
  }

  if (!status.ok())
  {
    FreeBuffer();
  }
  return status;
}

/**
 * @brief Stop the stream source.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Stop()
{
  // streaming stop
  senscord::Status status = device_.DevStop();
  if (!status.ok())
  {
    return SENSCORD_STATUS_TRACE(status);
  }

  status = FreeBuffer();
  if (!status.ok())
  {
    device_.DevStart();
    return SENSCORD_STATUS_TRACE(status);
  }

  status = device_.FreeReqBuffer();
  if (!status.ok())
  {
    AllocateBuffer();
    device_.DevStart();
    return SENSCORD_STATUS_TRACE(status);
  }

  is_started_ = false;
  return senscord::Status::OK();
}

/**
 * @brief Set parameter to device.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::SetDeviceParameter()
{
  senscord::Status status;
  {
    // set format
    status = device_.SetDevFormat(image_property_);
    if (!status.ok())
    {
      return SENSCORD_STATUS_TRACE(status);
    }

    // check device parameters
    senscord::ImageProperty device_param = {};
    status = device_.GetDevFormat(&device_param);
    if (!status.ok())
    {
      return SENSCORD_STATUS_TRACE(status);
    }
    SENSCORD_LOG_INFO("device width:%" PRIu32 ", height:%" PRIu32
                      ", stride_bytes:%" PRIu32 ", pixel_format:%s",
                      device_param.width, device_param.height,
                      device_param.stride_bytes, device_param.pixel_format.c_str());
    // YUYV is width*2. for NV16, it's two halves.
    if (device_param.pixel_format == senscord::kPixelFormatYUYV &&
        is_yuyv_to_nv16_)
    {
      device_param.stride_bytes = device_param.stride_bytes / 2;
      device_param.pixel_format = senscord::kPixelFormatNV16;
    }
    // correct device parameters
    image_property_ = device_param;
  }
  {
    // set framerate
    status = device_.SetFramerate(framerate_property_);
    if (!status.ok())
    {
      return SENSCORD_STATUS_TRACE(status);
    }
    // check device parameters
    senscord::FrameRateProperty device_param = {};
    status = device_.GetFramerate(&device_param);
    if (!status.ok())
    {
      return SENSCORD_STATUS_TRACE(status);
    }
    SENSCORD_LOG_INFO("device num:%" PRIu32 ", denom:%" PRIu32,
                      device_param.num, device_param.denom);
    // correct device parameters
    framerate_property_ = device_param;
  }
  // set request buffer
  status = device_.SetReqBuffer(settings_.buffer_num);
  if (!status.ok())
  {
    return SENSCORD_STATUS_TRACE(status);
  }
  return senscord::Status::OK();
}

/**
 * @brief Allocate device buffer.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::AllocateBuffer()
{
  senscord::Status status;
  for (uint32_t index = 0; index < settings_.buffer_num; ++index)
  {
    BufferInfo buffer = {};
    buffer.index = index;
    buffer.used = false;
    status = device_.Mmap(index, &buffer.addr, &buffer.length);
    if (!status.ok())
    {
      FreeBuffer();
      return SENSCORD_STATUS_TRACE(status);
    }
    senscord::Memory *memory = NULL;
    status = allocator_->Allocate(buffer.length, &memory);
    if (!status.ok())
    {
      FreeBuffer();
      return SENSCORD_STATUS_TRACE(status);
    }
    buffer.memory = memory;
    buffer_list_.push_back(buffer);
  }
  return senscord::Status::OK();
}

/**
 * @brief Free device buffer.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::FreeBuffer()
{
  while (!buffer_list_.empty())
  {
    BufferInfo buffer = buffer_list_.back();
    senscord::Status status = device_.Munmap(buffer.addr, buffer.length);
    if (!status.ok())
    {
      SENSCORD_LOG_WARNING(status.ToString().c_str());
    }
    allocator_->Free(buffer.memory);
    buffer_list_.pop_back();
  }
  return senscord::Status::OK();
}

/**
 * @brief Get property.
 * @param[in] (key) Property key.
 * @param[out] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Get(const std::string &key,
                                            senscord::ChannelInfoProperty *property)
{
  senscord::ChannelInfo channel_info = {};
  channel_info.raw_data_type = senscord::kRawDataTypeImage;
  channel_info.description = "Image data from a V4L2 device.";
  property->channels[0] = channel_info;
  return senscord::Status::OK();
}

/**
 * @brief Get property.
 * @param[in] (key) Property key.
 * @param[out] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Get(const std::string &key,
                                            senscord::ImageSensorFunctionSupportedProperty *property)
{
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
 * @brief Get property.
 * @param[in] (key) Property key.
 * @param[out] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Get(const std::string &key,
                                            senscord::FrameRateProperty *property)
{
  *property = framerate_property_;
  return senscord::Status::OK();
}

/**
 * @brief Set property.
 * @param[in] (key) Property key.
 * @param[in] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Set(const std::string &key,
                                            const senscord::FrameRateProperty *property)
{
  if (is_started_)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidOperation,
                                "already streaming");
  }
  framerate_property_ = *property;
  return senscord::Status::OK();
}

/**
 * @brief Get property.
 * @param[in] (key) Property key.
 * @param[out] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Get(const std::string &key,
                                            senscord::ImageProperty *property)
{
  *property = image_property_;
  return senscord::Status::OK();
}

/**
 * @brief Set property.
 * @param[in] (key) Property key.
 * @param[in] (property) Property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Set(const std::string &key,
                                            const senscord::ImageProperty *property)
{
  if (is_started_)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidOperation,
                                "already streaming");
  }
  image_property_ = *property;
  return senscord::Status::OK();
}

/**
 * @brief Get property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Get(const std::string &key,
                                            V4L2SampleProperty *property)
{
  SENSCORD_LOG_INFO("Get V4L2SampleProperty by key=%s prop=%p",
                    key.c_str(), property);
  property->a = 100;
  property->b = 200;
  property->c = "v4l2";
  return senscord::Status::OK();
}

/**
 * @brief Set property.
 * @return Status object.
 */
senscord::Status V4L2ImageStreamSource::Set(const std::string &key,
                                            const V4L2SampleProperty *property)
{
  SENSCORD_LOG_INFO("Set V4L2SampleProperty by key=%s prop=%p",
                    key.c_str(), property);
  return senscord::Status::OK();
}

/**
 * @brief Parse parameter.
 */
void V4L2ImageStreamSource::ParseParameter()
{
  senscord::Status status;

  {
    // device name
    std::string value;
    status = util_->GetInstanceArgument("device", &value);
    if (status.ok())
    {
      settings_.device = value;
    }

    // pixel_format
    status = util_->GetInstanceArgument("pixel_format", &value);
    if (status.ok())
    {
      image_property_.pixel_format = value;
    }

    // yuyv_to_nv16
    status = util_->GetInstanceArgument("yuyv_to_nv16", &value);
    if (status.ok())
    {
      is_yuyv_to_nv16_ = (value == "false" ? false : true);
    }
  }

  {
    // buffer num
    uint64_t num = 0;
    status = util_->GetInstanceArgument("buffer_num", &num);
    if (status.ok())
    {
      settings_.buffer_num = static_cast<uint32_t>(num);
    }

    // width
    status = util_->GetInstanceArgument("width", &num);
    if (status.ok())
    {
      image_property_.width = static_cast<uint32_t>(num);
      image_property_.stride_bytes = static_cast<uint32_t>(num);
    }

    // height
    status = util_->GetInstanceArgument("height", &num);
    if (status.ok())
    {
      image_property_.height = static_cast<uint32_t>(num);
    }

    // stride_bytes
    status = util_->GetInstanceArgument("stride_bytes", &num);
    if (status.ok())
    {
      image_property_.stride_bytes = static_cast<uint32_t>(num);
    }

    // framerate
    status = util_->GetInstanceArgument("framerate", &num);
    if (status.ok())
    {
      framerate_property_.num = static_cast<uint32_t>(num);
      framerate_property_.denom = 1;
    }
  }
}

/**
 * @brief Get used buffer number.
 * @return used buffer number.
 */
uint32_t V4L2ImageStreamSource::GetUsedBufferNum() const
{
  uint32_t cnt = 0;
  senscord::osal::OSLockMutex(buffer_mutex_);
  for (size_t index = 0; index < buffer_list_.size(); ++index)
  {
    if (buffer_list_[index].used)
    {
      ++cnt;
    }
  }
  senscord::osal::OSUnlockMutex(buffer_mutex_);
  return cnt;
}

/**
 * @brief Get nano sec timestamp.
 * @param[in] (time) Structure to convert nano sec.
 * @return nano sec timestamp.
 */
uint64_t V4L2ImageStreamSource::GetNsecTimestamp(
    const struct timeval &time) const
{
  return ((static_cast<uint64_t>(time.tv_sec) * 1000 * 1000 * 1000) +
          (time.tv_usec * 1000));
}
