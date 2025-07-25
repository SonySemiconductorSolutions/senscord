/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_C_API_SENSCORD_C_TYPES_H_
#define SENSCORD_C_API_SENSCORD_C_TYPES_H_

#include <stdint.h>
#include <stddef.h>

#include "senscord/config.h"
#include "senscord/c_api/property_c_types.h"

typedef uint64_t senscord_handle_t;

/** Core handle. */
typedef senscord_handle_t senscord_core_t;
/** Stream handle. */
typedef senscord_handle_t senscord_stream_t;
/** Frame handle. */
typedef senscord_handle_t senscord_frame_t;
/** Channel handle. */
typedef senscord_handle_t senscord_channel_t;
/** Event argument handle. */
typedef senscord_handle_t senscord_event_argument_t;
/** Config handle. */
typedef senscord_handle_t senscord_config_t;
/** TemporalContrastDataReader handle. */
typedef senscord_handle_t senscord_temporal_contrast_data_reader_t;
/** Property lock resource. */
typedef senscord_handle_t senscord_property_lock_resource_t;

/**
 * @brief Frame received callback function.
 * @param[in] stream        Stream handle.
 * @param[in] private_data  Private data.
 * @see senscord_stream_register_frame_callback
 */
typedef void (*senscord_frame_received_callback)(
    senscord_stream_t stream, void* private_data);

/**
 * @brief Event received callback function.
 * @param[in] event_type    Event type to receive.
 * @param[in] reserved      Not used.
 * @param[in] private_data  Private data.
 * @see senscord_stream_register_event_callback
 * @see SENSCORD_EVENT_ERROR, SENSCORD_EVENT_FRAME_DROPPED, etc.
 */
typedef void (*senscord_event_received_callback)(
    const char* event_type, const void* reserved, void* private_data);

/**
 * @brief Event received callback function.
 * @param[in] stream        Stream handle.
 * @param[in] event_type    Event type to receive.
 * @param[in] args          Event argument handle.
 * @param[in] private_data  Private data.
 * @see senscord_stream_register_event_callback2
 * @see SENSCORD_EVENT_ERROR, SENSCORD_EVENT_FRAME_DROPPED, etc.
 */
typedef void (*senscord_event_received_callback2)(
    senscord_stream_t stream, const char* event_type,
    senscord_event_argument_t args, void* private_data);

// default allocator key
#define SENSCORD_CONFIG_DEFAULT_ALLOCATOR_KEY "_default"

// allocator type
#define SENSCORD_ALLOCATOR_TYPE_HEAP "heap"
#ifdef SENSCORD_ALLOCATOR_SHARED_MEMORY
#define SENSCORD_ALLOCATOR_TYPE_SHARED_MEMORY "shared_memory"
#endif  /* SENSCORD_ALLOCATOR_SHARED_MEMORY */

// allocator name
#define SENSCORD_ALLOCATOR_NAME_DEFAULT ""

/**
 * @brief Stream types
 * @see senscord::kStreamTypeImage, senscord::kStreamTypeDepth, etc.
 */
#define SENSCORD_STREAM_TYPE_IMAGE  "image"
#define SENSCORD_STREAM_TYPE_DEPTH  "depth"
#define SENSCORD_STREAM_TYPE_IMU    "imu"
#define SENSCORD_STREAM_TYPE_SLAM   "slam"
#define SENSCORD_STREAM_TYPE_OBJECT_DETECTION   "object_detection"
#define SENSCORD_STREAM_TYPE_KEY_POINT          "key_point"
#define SENSCORD_STREAM_TYPE_TEMPORAL_CONTRAST  "pixel_polarity"
#define SENSCORD_STREAM_TYPE_OBJECT_TRACKING    "object_tracking"
#define SENSCORD_STREAM_TYPE_AUDIO  "audio"
/**
 * @deprecated will be replaced by SENSCORD_STREAM_TYPE_TEMPORAL_CONTRAST
 */
#define SENSCORD_STREAM_TYPE_PIXEL_POLAITY      "pixel_polarity"

/**
 * @brief The information of stream key.
 * @see senscord::StreamTypeInfo
 */
struct senscord_stream_type_info_t {
  const char* key;   /**< Stream key. */
  const char* type;  /**< Stream type. */
  const char* id;    /**< ID. */
};

/**
 * @brief Type of stream information.
 */
enum senscord_stream_info_param_t {
  SENSCORD_STREAM_INFO_STREAM_KEY,
  SENSCORD_STREAM_INFO_STREAM_TYPE,
  SENSCORD_STREAM_INFO_IDENTIFICATION,
};

/**
 * @brief Frame buffering setting.
 * @see senscord::FrameBuffering
 */
typedef struct senscord_frame_buffering_property_t senscord_frame_buffering_t;

#define SENSCORD_STREAM_ARGUMENT_LIST_MAX     (32)
#define SENSCORD_STREAM_ARGUMENT_NAME_LENGTH  (32)
#define SENSCORD_STREAM_ARGUMENT_VALUE_LENGTH (256)

/**
 * @brief Stream argument element.
 */
struct senscord_stream_argument_t {
  char name[SENSCORD_STREAM_ARGUMENT_NAME_LENGTH];    /**< Name. */
  char value[SENSCORD_STREAM_ARGUMENT_VALUE_LENGTH];  /**< Value. */
};

/**
 * @brief Open stream setting.
 * @see senscord::OpenStreamSetting
 */
struct senscord_open_stream_setting_t {
  /** Frame buffering setting. */
  senscord_frame_buffering_t frame_buffering;
  /** Count of the stream argument array. */
  uint32_t arguments_count;
  /** Stream argument array. */
  struct senscord_stream_argument_t
      arguments[SENSCORD_STREAM_ARGUMENT_LIST_MAX];
};

/**
 * @brief Maximum number of register access elements.
 *
 * It is used as the maximum value of the second argument 'count' of
 * senscord_register_accessor_create() function.
 */
#define SENSCORD_MAX_REGISTER_ACCESS_ELEMENT  ((size_t)0x10000)

/**
 * @brief Information for single register access.
 * @see senscord::RegisterAccessElement
 */
struct senscord_register_access_element_t {
  uintptr_t address;  /**< Target address. */
  size_t data_size;   /**< Read/Write data size. */
  uint8_t* data;      /**< Writing data or read data. */
};

/**
 * @brief User data informations.
 * @see senscord::Frame::UserData
 */
struct senscord_user_data_t {
  void* address;  /**< virtual address */
  size_t size;    /**< data size */
};

/**
 * @brief Raw data informations.
 * @see senscord::Channel::RawData
 */
struct senscord_raw_data_t {
  void* address;       /**< virtual address */
  size_t size;         /**< data size */
  const char* type;    /**< data type*/
  uint64_t timestamp;  /**< nanoseconds timestamp captured by the device */
};

/** stream with no destination. */
#define SENSCORD_DESTINATION_STREAM_NONE  (-1)

/**
 * @brief Stream version information.
 * @see senscord::StreamVersion
 */
struct senscord_stream_version_t {
  char stream_key[SENSCORD_STREAM_KEY_LENGTH];        /**< Stream key */
  struct senscord_version_property_t stream_version;  /**< Stream version. */
  uint32_t  linkage_count;  /**< Number of linkage versions */
  /** Stream linkage versions */
  struct senscord_version_property_t* linkage_versions;
  int32_t destination_id;   /**< Destination ID. */
};

/**
 * @brief SensCord version information.
 * @see senscord::SensCordVersion
 */
struct senscord_version_t {
  struct senscord_version_property_t senscord_version;  /**< Senscord version */
  struct senscord_version_property_t project_version;   /**< Project version */
  uint32_t  stream_count;         /**< Number of streams */
  /** Stream versions */
  struct senscord_stream_version_t* stream_versions;
  int32_t destination_id;         /**< Destination ID */
  uint32_t  server_count;         /**< Number of servers */
  struct senscord_version_t* server_versions;  /**< Server versions */
};

/**
 * Event definitions
 * @see senscord::EventAny, senscord::kEventError, etc.
 */
#define SENSCORD_EVENT_ANY              "EventAny"
#define SENSCORD_EVENT_ERROR            "EventError"
#define SENSCORD_EVENT_FATAL            "EventFatal"
#define SENSCORD_EVENT_FRAME_DROPPED    "EventFrameDropped"
#define SENSCORD_EVENT_PROPERTY_UPDATED "EventPropertyUpdated"
#define SENSCORD_EVENT_PLUGGED          "EventPlugged"
#define SENSCORD_EVENT_UNPLUGGED        "EventUnplugged"
#define SENSCORD_EVENT_RECORD_STATE     "EventRecordState"

#define SENSCORD_EVENT_ARGUMENT_CAUSE           "cause"
#define SENSCORD_EVENT_ARGUMENT_MESSAGE         "message"
#define SENSCORD_EVENT_ARGUMENT_SEQUENCE_NUMBER "sequence_number"
#define SENSCORD_EVENT_ARGUMENT_PROPERTY_KEY    "property_key"
#define SENSCORD_EVENT_ARGUMENT_RECORD_STATE    "state"
#define SENSCORD_EVENT_ARGUMENT_RECORD_COUNT    "count"
#define SENSCORD_EVENT_ARGUMENT_RECORD_PATH     "path"

/**
 * Channel ID definitions
 * @see senscord::kChannelIdBase, senscord::kChannelIdVendorBase, etc.
 */
#define SENSCORD_CHANNEL_ID_BASE  (0)
#define SENSCORD_CHANNEL_ID_VENDOR_BASE  (0x80000000)

/**
 * Channel ID (Image frame) definitions
 * @see senscord::kChannelIdImage.
 */
#define SENSCORD_CHANNEL_ID_IMAGE(index)  ((SENSCORD_CHANNEL_ID_BASE) + (index))

/**
 * Channel ID (Depth frame) definitions
 * @see senscord::kChannelIdDepth, senscord::kChannelIdDepthConfidence, etc.
 */
#define SENSCORD_CHANNEL_ID_DEPTH(index) \
  ((SENSCORD_CHANNEL_ID_BASE) + ((index) * 3) + 0)

#define SENSCORD_CHANNEL_ID_DEPTH_CONFIDENCE(index) \
  ((SENSCORD_CHANNEL_ID_BASE) + ((index) * 3) + 1)

#define SENSCORD_CHANNEL_ID_DEPTH_POINT_CLOUD(index) \
  ((SENSCORD_CHANNEL_ID_BASE) + ((index) * 3) + 2)

/**
 * Channel ID (SLAM frame) definitions
 * @see senscord::kChannelIdSlamPose, senscord::kChannelIdSlamPointCloud, etc.
 */
#define SENSCORD_CHANNEL_ID_SLAM_POSE         ((SENSCORD_CHANNEL_ID_BASE) + 0)
#define SENSCORD_CHANNEL_ID_SLAM_POINT_CLOUD  ((SENSCORD_CHANNEL_ID_BASE) + 1)
#define SENSCORD_CHANNEL_ID_SLAM_GRID_MAP     ((SENSCORD_CHANNEL_ID_BASE) + 2)

/**
 * Channel ID (SLAM frame) definitions
 * @see senscord::kChannelIdImuAcceleration, senscord::kChannelIdImuAngularVelocity, etc.
 */
#define SENSCORD_CHANNEL_ID_IMU_ACCELERATION \
  ((SENSCORD_CHANNEL_ID_BASE) + 0)
#define SENSCORD_CHANNEL_ID_IMU_ANGULAR_VELOCITY \
  ((SENSCORD_CHANNEL_ID_BASE) + 1)
#define SENSCORD_CHANNEL_ID_IMU_MAGUNETIC_FIELD \
  ((SENSCORD_CHANNEL_ID_BASE) + 2)

/**
 * Channel ID definitions
 * @see senscord::kChannelIdBase
 */
#define SENSCORD_CHANNEL_ID_BASE  (0)

/**
 * Channel ID (ObjectDetection frame) definitions
 * @see senscord::kChannelIdObjectDetecion
 */
#define SENSCORD_CHANNEL_ID_OBJECT_DETECTION  ((SENSCORD_CHANNEL_ID_BASE) + 0)

/**
* Channel ID (KeyPoint frame) definitions
* @see senscord::kChannelIdKeyPoint
*/
#define SENSCORD_CHANNEL_ID_KEY_POINT         ((SENSCORD_CHANNEL_ID_BASE) + 0)


/**
 * Channel ID (TemporalContrast frame) definitions
 * @see senscord::kChannelIdTemporalContrastData,
 *      senscord::kChannelIdTemporalContrastImage
 */
#define SENSCORD_CHANNEL_ID_TEMPORAL_CONTRAST_DATA \
  ((SENSCORD_CHANNEL_ID_BASE) + 0)
#define SENSCORD_CHANNEL_ID_TEMPORAL_CONTRAST_IMAGE \
  ((SENSCORD_CHANNEL_ID_BASE) + 1)

/**
 * Channel ID (PixelPolarity frame) definitions
 * @see senscord::kChannelIdPixelPolarityData,
 *      senscord::kChannelIdPixelPolarityImage
 * @deprecated will be replaced by SENSCORD_CHANNEL_ID_TEMPORAL_CONTRAST_DATA,
 *             SENSCORD_CHANNEL_ID_TEMPORAL_CONTRAST_IMAGE
 */
#define SENSCORD_CHANNEL_ID_PIXEL_POLARITY_DATA \
  ((SENSCORD_CHANNEL_ID_BASE) + 0)
#define SENSCORD_CHANNEL_ID_PIXEL_POLARITY_IMAGE \
  ((SENSCORD_CHANNEL_ID_BASE) + 1)

/**
 * Channel ID (ObjectTracking frame) definitions
 * @see senscord::kChannelIdObjectTracking
 */
#define SENSCORD_CHANNEL_ID_OBJECT_TRACKING  ((SENSCORD_CHANNEL_ID_BASE) + 0)

/**
 * Timeout definitions
 * @see senscord::kTimeoutPolling, senscord::kTimeoutForever
 */
#define SENSCORD_TIMEOUT_POLLING  ((int32_t)0)
#define SENSCORD_TIMEOUT_FOREVER  ((int32_t)-1)

#endif  /* SENSCORD_C_API_SENSCORD_C_TYPES_H_ */
