/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/v4l2_accessor.h"

#include <inttypes.h>
#include <sys/ioctl.h> /* ioctl */
#include <sys/mman.h>  /* mmap */
#include <fcntl.h>     /* oepn */
#include <unistd.h>    /* close */
#include <string>

#include "senscord/osal.h"

namespace
{
  const char *kBlockName = "v4l2_accessor";

  /*
   * Pixel format element
   */
  struct PixelFormat
  {
    uint32_t v4l2_format;
    std::string senscord_format;
  };

  /*
   * Pixel format list (V4L2 / SensCord)
   */
  const PixelFormat PixelFormatList[] = {
      {V4L2_PIX_FMT_GREY, senscord::kPixelFormatGREY},
      {V4L2_PIX_FMT_YUYV, senscord::kPixelFormatYUYV},
      {V4L2_PIX_FMT_UYVY, senscord::kPixelFormatUYVY},
      {V4L2_PIX_FMT_NV16, senscord::kPixelFormatNV16},
      {V4L2_PIX_FMT_RGB24, senscord::kPixelFormatRGB24},
      {V4L2_PIX_FMT_MJPEG, senscord::kPixelFormatJPEG},
  };
} // namespace

/**
 * @brief Constructor.
 */
V4L2Accessor::V4L2Accessor()
{
  fd_ = -1;
}

/**
 * @brief Destructor.
 */
V4L2Accessor::~V4L2Accessor()
{
  if (fd_ != -1)
  {
    DevClose();
  }
}

/**
 * @brief Device open.
 * @param[in] (path) device path.
 * @return The status of function.
 */
senscord::Status V4L2Accessor::DevOpen(const std::string &path)
{
  fd_ = open(path.c_str(), O_RDWR);
  if (fd_ < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed open device: path:%s, errno:%d", path.c_str(), errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Device close.
 * @return The status of function.
 */
senscord::Status V4L2Accessor::DevClose()
{
  int32_t ret = close(fd_);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed close device: errno:%d", errno);
  }
  fd_ = -1;
  return senscord::Status::OK();
}

/**
 * @brief Get format of device.
 * @param[out] (property) image property.
 * @return Status object.
 */
senscord::Status V4L2Accessor::GetDevFormat(
    senscord::ImageProperty *property)
{
  struct v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  int32_t ret = ioctl(fd_, VIDIOC_G_FMT, &fmt);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed get format: errno:%d", errno);
  }
  property->width = fmt.fmt.pix.width;
  property->height = fmt.fmt.pix.height;
  property->stride_bytes = fmt.fmt.pix.bytesperline;
  senscord::Status status = GetSensCordPixelFormat(
      fmt.fmt.pix.pixelformat, &property->pixel_format);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set format to device.
 * @param[in] (property) image property.
 * @return Status object.
 */
senscord::Status V4L2Accessor::SetDevFormat(
    const senscord::ImageProperty &property)
{
  struct v4l2_format fmt = {};
  senscord::Status status = GetV4L2PixelFormat(
      property.pixel_format, &fmt.fmt.pix.pixelformat);
  if (!status.ok())
  {
    return SENSCORD_STATUS_TRACE(status);
  }
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = property.width;
  fmt.fmt.pix.height = property.height;
  fmt.fmt.pix.bytesperline = property.stride_bytes;
  fmt.fmt.pix.field = V4L2_FIELD_ANY;

  int32_t ret = ioctl(fd_, VIDIOC_S_FMT, &fmt);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed set format: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Get framerate to device.
 * @param[out] (property) framerate property.
 * @return Status object.
 */
senscord::Status V4L2Accessor::GetFramerate(
    senscord::FrameRateProperty *property)
{
  struct v4l2_streamparm parm = {};

  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int32_t ret = ioctl(fd_, VIDIOC_G_PARM, &parm);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed get framerate: errno:%d", errno);
  }
  // In V4L2, it is reciprocal.
  property->num = parm.parm.capture.timeperframe.denominator;
  property->denom = parm.parm.capture.timeperframe.numerator;
  return senscord::Status::OK();
}

/**
 * @brief Set framerate to device.
 * @param[in] (property) framerate property.
 * @return Status object.
 */
senscord::Status V4L2Accessor::SetFramerate(
    const senscord::FrameRateProperty &property)
{
  struct v4l2_streamparm parm = {};

  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(fd_, VIDIOC_G_PARM, &parm);
  // In V4L2, it is reciprocal.
  parm.parm.capture.timeperframe.numerator = property.denom;
  parm.parm.capture.timeperframe.denominator = property.num;

  int32_t ret = ioctl(fd_, VIDIOC_S_PARM, &parm);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed set framerate: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Set request buffer to device.
 * @param[in] (num_req) request number of buffer.
 * @return Status object.
 */
senscord::Status V4L2Accessor::SetReqBuffer(uint32_t num_req)
{
  struct v4l2_requestbuffers reqbuf = {};
  reqbuf.count = num_req;
  reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbuf.memory = V4L2_MEMORY_MMAP;

  int32_t ret = ioctl(fd_, VIDIOC_REQBUFS, &reqbuf);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed request buffer: num:%" PRIu32 ", errno:%d", num_req, errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Free buffer to device.
 * @return Status object.
 */
senscord::Status V4L2Accessor::FreeReqBuffer()
{
  struct v4l2_requestbuffers reqbuf = {};
  reqbuf.count = 0;
  reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbuf.memory = V4L2_MEMORY_MMAP;

  int32_t ret = ioctl(fd_, VIDIOC_REQBUFS, &reqbuf);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed free request buffer: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Query buffer.
 * @param[in] (index) Inquiry buffer index.
 * @param[out] (buffer) Buffer of inquiry result.
 * @return Status object.
 */
senscord::Status V4L2Accessor::QueryBuffer(
    const uint32_t index, struct v4l2_buffer *buffer)
{
  if (buffer == NULL)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidArgument, "buffer is NULL");
  }
  senscord::osal::OSMemset(buffer, 0, sizeof(*buffer));
  buffer->index = index;
  buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  int32_t ret = ioctl(fd_, VIDIOC_QUERYBUF, buffer);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed query buffer: index:%" PRIu32 ", errno:%d", index, errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Queue buffer.
 * @param[in] (buffer) Target queue buffer.
 * @return Status object.
 */
senscord::Status V4L2Accessor::QueueBuffer(v4l2_buffer *buffer)
{
  if (buffer == NULL)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidArgument, "buffer is NULL");
  }
  int32_t ret = ioctl(fd_, VIDIOC_QBUF, buffer);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed queue buffer: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Dequeue buffer.
 * @param[out] (buffer) Buffer of dequeue result.
 * @return Status object.
 */
senscord::Status V4L2Accessor::DequeueBuffer(v4l2_buffer *buffer)
{
  if (buffer == NULL)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseInvalidArgument, "buffer is NULL");
  }
  senscord::osal::OSMemset(buffer, 0, sizeof(*buffer));
  buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer->memory = V4L2_MEMORY_MMAP;

  int32_t ret = ioctl(fd_, VIDIOC_DQBUF, buffer);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed dequeue buffer: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Mmap to device.
 * @param[in] (index) Buffer index.
 * @param[out] (addr) Mmap address.
 * @param[out] (size) Mmap size
 * @return Status object.
 */
senscord::Status V4L2Accessor::Mmap(
    uint32_t index, void **addr, uint32_t *size)
{
  struct v4l2_buffer buffer = {};

  senscord::Status status = QueryBuffer(index, &buffer);
  if (!status.ok())
  {
    return SENSCORD_STATUS_TRACE(status);
  }
  void *mmap_addr = mmap(NULL,
                         buffer.length,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         fd_, buffer.m.offset);
  if (mmap_addr == MAP_FAILED)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed mmap: index:%" PRIu32 ", errno:%d", index, errno);
  }
  *addr = mmap_addr;
  *size = buffer.length;
  return senscord::Status::OK();
}

/**
 * @brief Munmap to device.
 * @param[in] (addr) Mmap address.
 * @param[in] (size) Mmap size.
 * @return Status object.
 */
senscord::Status V4L2Accessor::Munmap(void *addr, uint32_t size)
{
  if (addr != NULL)
  {
    int32_t ret = munmap(addr, size);
    if (ret != 0)
    {
      return SENSCORD_STATUS_FAIL(kBlockName,
                                  senscord::Status::kCauseAborted,
                                  "failed munmap: errno:%d", errno);
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Set start stream to device.
 * @return Status object.
 */
senscord::Status V4L2Accessor::DevStart()
{
  uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int32_t ret = ioctl(fd_, VIDIOC_STREAMON, &type);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed start streaming: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Set stop stream to device.
 * @return Status object.
 */
senscord::Status V4L2Accessor::DevStop()
{
  uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int32_t ret = ioctl(fd_, VIDIOC_STREAMOFF, &type);
  if (ret < 0)
  {
    return SENSCORD_STATUS_FAIL(kBlockName,
                                senscord::Status::kCauseAborted,
                                "failed stop streaming: errno:%d", errno);
  }
  return senscord::Status::OK();
}

/**
 * @brief Get v4l2 pixelformat from senscord pixel format.
 * @param[in] (format) SensCord pixel format.
 * @param[out] (converted) V4L2 pixel format.
 * @return Status object.
 */
senscord::Status V4L2Accessor::GetV4L2PixelFormat(
    const std::string &format, uint32_t *converted) const
{
  uint32_t size = sizeof(PixelFormatList) / sizeof(PixelFormatList[0]);
  for (uint32_t index = 0; index < size; ++index)
  {
    if (PixelFormatList[index].senscord_format == format)
    {
      *converted = PixelFormatList[index].v4l2_format;
      return senscord::Status::OK();
    }
  }
  return SENSCORD_STATUS_FAIL(kBlockName,
                              senscord::Status::kCauseNotSupported,
                              "not supported format:%s", format.c_str());
}

/**
 * @brief Get senscord pixelformat from v4l2 pixel format.
 * @param[in] (format) V4L2 pixel format.
 * @param[out] (converted) SensCord pixel format.
 * @return Status object.
 */
senscord::Status V4L2Accessor::GetSensCordPixelFormat(
    const uint32_t format, std::string *converted) const
{
  uint32_t size = sizeof(PixelFormatList) / sizeof(PixelFormatList[0]);
  for (uint32_t index = 0; index < size; ++index)
  {
    if (PixelFormatList[index].v4l2_format == format)
    {
      *converted = PixelFormatList[index].senscord_format;
      return senscord::Status::OK();
    }
  }
  return SENSCORD_STATUS_FAIL(kBlockName,
                              senscord::Status::kCauseNotSupported,
                              "not supported format:%" PRIu32, format);
}
