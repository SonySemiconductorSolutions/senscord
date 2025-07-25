/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utility> // make_pair
#include "senscord/develop/standard_component.h"
#include "src/v4l2_image_stream_source.h"

/**
 * @brief The factory of stream sources for pseudo image component.
 */
class V4L2ImageStreamSourceFactory : public senscord::StreamSourceFactory
{
public:
  /**
   * @brief Get the List of supported types.
   * @param[in] (args) Arguments written by senscord.xml.
   * @param[out] (list) List of supported types.
   */
  virtual void GetSupportedList(
      const senscord::ComponentArgument &args, SourceTypeList *list)
  {
    list->push_back(std::make_pair(senscord::kStreamTypeImage, 0));
  }

  /**
   * @brief Create the stream source on the component.
   * @param[in] (type) Type of creating source.
   * @param[out] (source) Stream source.
   * @return Status object.
   */
  virtual senscord::Status CreateSource(
      const SourceType &type, senscord::StreamSource **source)
  {
    if (type.first == senscord::kStreamTypeImage)
    {
      *source = new V4L2ImageStreamSource();
    }
    return senscord::Status::OK();
  }
};

// register
SENSCORD_REGISTER_COMPONENT(V4L2ImageStreamSourceFactory)
