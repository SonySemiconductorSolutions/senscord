/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PSEUDO_IMAGE_SRC_PSEUDO_IMAGE_SOURCE_H_
#define LIB_COMPONENT_PSEUDO_IMAGE_SRC_PSEUDO_IMAGE_SOURCE_H_

#include <string>
#include <vector>
#include "senscord/logger.h"
#include "senscord/osal.h"
#include "senscord/develop/stream_source.h"
#include "senscord/pseudo_image/pseudo_image_types.h"

/**
 * @brief The stream source of pseudo image (new style).
 */
class PseudoImageSource : public senscord::ImageStreamSource {
 public:
  /**
   * @brief Open the stream source.
   * @param[in] (core) The core instance.
   * @param[in] (util) The utility accessor to core.
   * @return The status of function.
   */
  virtual senscord::Status Open(
      senscord::Core* core,
      senscord::StreamSourceUtility* util);

  /**
   * @brief Close the stream source.
   * @return The status of function.
   */
  virtual senscord::Status Close();

  /**
   * @brief Pull up the new frames.
   * @param[out] (frames) The information about new frames.
   */
  virtual void GetFrames(std::vector<senscord::FrameInfo>* frames);

  /**
   * @brief Release the used frame.
   * @param[in] (frameinfo) The information about used frame.
   * @param[in] (referenced_channel_ids) List of referenced channel IDs.
   *                                     (NULL is the same as empty)
   * @return The status of function.
   */
  virtual senscord::Status ReleaseFrame(
    const senscord::FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids);

  /// Mandatory properties.

  /**
   * @brief Get the stream source property.
   * @param[in] (key) The key of property.
   * @param[out] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Get(
    const std::string& key, senscord::ChannelInfoProperty* property);

  /**
   * @brief Get the stream source property.
   * @param[in] (key) The key of property.
   * @param[out] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Get(
    const std::string& key, senscord::FrameRateProperty* property);

  /**
   * @brief Set the new stream source property.
   * @param[in] (key) The key of property.
   * @param[in] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Set(
    const std::string& key, const senscord::FrameRateProperty* property);

  /**
   * @brief Get the stream source property.
   * @param[in] (key) The key of property.
   * @param[out] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Get(
    const std::string& key, senscord::ImageProperty* property);

  /**
   * @brief Set the new stream source property.
   * @param[in] (key) The key of property.
   * @param[in] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Set(
    const std::string& key, const senscord::ImageProperty* property);

  /**
   * @brief Get the stream source property.
   * @param[in] (key) The key of property.
   * @param[out] (property) The location of property.
   * @return The status of function.
   */
  virtual senscord::Status Get(
    const std::string& key,
    senscord::ImageSensorFunctionSupportedProperty* property);

  /// Original properties.

  /**
   * @brief Get the stream source property.
   * @param[in] (key) The key of property.
   * @param[out] (property) The location of property.
   * @return The status of function.
   */
  senscord::Status Get(
    const std::string& key, PseudoImageProperty* property);

  /**
   * @brief Set the new stream source property.
   * @param[in] (key) The key of property.
   * @param[in] (property) The location of property.
   * @return The status of function.
   */
  senscord::Status Set(
    const std::string& key, const PseudoImageProperty* property);

  /**
   * @brief Constructor
   */
  PseudoImageSource();

  /**
   * @brief Destructor
   */
  ~PseudoImageSource();

 private:
  senscord::StreamSourceUtility* util_;
  senscord::MemoryAllocator* allocator_;
  uint64_t frame_seq_num_;
  uint64_t sleep_nsec_;

  // properties
  senscord::FrameRateProperty framerate_;
  senscord::ImageProperty image_property_;
  PseudoImageProperty pseudo_image_;
};

#endif    // LIB_COMPONENT_PSEUDO_IMAGE_SRC_PSEUDO_IMAGE_SOURCE_H_
