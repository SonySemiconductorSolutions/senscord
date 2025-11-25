/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_C_API_PROPERTY_C_TYPES_H_
#define SENSCORD_C_API_PROPERTY_C_TYPES_H_

#include <stdint.h>

#include "senscord/config.h"
#include "senscord/c_api/property_c_types_audio.h"
#ifdef __wasm__
#include "senscord/c_api/property_wasm_types.h"
#endif  /* __wasm__ */

/** Length of the version string. */
#define SENSCORD_VERSION_LENGTH         (256)
/** Length of the stream type string. */
#define SENSCORD_STREAM_TYPE_LENGTH     (64)
/** Length of the stream key string. */
#define SENSCORD_STREAM_KEY_LENGTH      (64)
/** Length of the raw data type string. */
// TODO: Temporarily force to use AITRIOS_SPECIFIC definition.
// #ifdef AITRIOS_SPECIFIC
#define SENSCORD_RAWDATA_TYPE_LENGTH    (16)
// #else
// #define SENSCORD_RAWDATA_TYPE_LENGTH    (64)
// #endif
/** Length of the pixel format string. */
#define SENSCORD_PIXEL_FORMAT_LENGTH    (64)
/** Length of the recorder format string. */
#define SENSCORD_RECORDER_FORMAT_LENGTH (64)
/** Length of the record directory type string. */
#define SENSCORD_RECORD_DIRECTORY_TYPE_LENGTH   (64)
/** Length of the record name rule string. */
#define SENSCORD_RECORD_NAME_RULE_LENGTH    (128)
/** Length of the file path string. */
#define SENSCORD_FILE_PATH_LENGTH       (256)
/** Length of the record date string. */
#define SENSCORD_RECORD_DATE_LENGTH     (32)

/** Maximum number of channel list. */
// #ifdef AITRIOS_SPECIFIC
#define SENSCORD_CHANNEL_LIST_MAX       (8)
// #else
// #define SENSCORD_CHANNEL_LIST_MAX       (128)
// #endif
/** Maximum number of preset list. */
#define SENSCORD_PRESET_LIST_MAX        (64)
/** Maximum number of recorder format list. */
#define SENSCORD_RECORDER_FORMAT_LIST_MAX   (64)
/** Maximum number of temperature list. */
#define SENSCORD_TEMPERATURE_LIST_MAX   (64)
/** Length of the record name rule list. */
#define SENSCORD_RECORD_NAME_RULE_LIST_MAX  (8)

/** Length of description string of channel. */
// #ifdef AITRIOS_SPECIFIC
#define SENSCORD_CHANNEL_DESCRIPTION_LENGTH (32)
// #else
// #define SENSCORD_CHANNEL_DESCRIPTION_LENGTH (128)
// #endif
/** Length of description string of preset. */
#define SENSCORD_PRESET_DESCRIPTION_LENGTH  (128)
/** Length of description string of temperature. */
#define SENSCORD_TEMPERATURE_DESCRIPTION_LENGTH   (128)

/** Length of the exposure mode string. */
#define SENSCORD_EXPOSURE_MODE_LENGTH       (64)
/** Length of the exposure metering string. */
#define SENSCORD_EXPOSURE_METERING_LENGTH   (64)
/** Length of the white balance mode string. */
#define SENSCORD_WHITE_BALANCE_MODE_LENGTH  (64)

/** Length of the data rate element list. */
#define SENSCORD_DATA_RATE_ELEMENT_LIST_MAX (64)
/** Length of the data rate name string. */
#define SENSCORD_DATE_RATE_NAME_LENGTH (64)
/** Length of the data rate unit string. */
#define SENSCORD_DATE_RATE_UNIT_LENGTH (64)

/**
 * Version property
 * @see senscord::kVersionPropertyKey
 */
#define SENSCORD_VERSION_PROPERTY_KEY   "version_property"

/**
 * @brief Version information.
 * @see senscord::VersionProperty
 */
struct senscord_version_property_t {
  char name[SENSCORD_VERSION_LENGTH];         /**< Name. */
  uint32_t major;                             /**< Major version. */
  uint32_t minor;                             /**< Minor version. */
  uint32_t patch;                             /**< Patch version. */
  char description[SENSCORD_VERSION_LENGTH];  /**< Version description. */
};

/**
 * StreamTypeProperty
 * @see senscord::kStreamTypePropertyKey
 */
#define SENSCORD_STREAM_TYPE_PROPERTY_KEY   "stream_type_property"

/**
 * @brief Property for the type of the stream.
 * @see senscord::StreamTypeProperty
 */
struct senscord_stream_type_property_t {
  char type[SENSCORD_STREAM_TYPE_LENGTH];
};

/**
 * StreamKeyProperty
 * @see senscord::kStreamKeyPropertyKey
 */
#define SENSCORD_STREAM_KEY_PROPERTY_KEY    "stream_key_property"

/**
 * @brief Property for the key of the stream.
 * @see senscord::StreamKeyProperty
 */
struct senscord_stream_key_property_t {
  char stream_key[SENSCORD_STREAM_KEY_LENGTH];
};

/**
 * StreamStateProperty
 * @see senscord::kStreamStatePropertyKey
 */
#define SENSCORD_STREAM_STATE_PROPERTY_KEY  "stream_state_property"

/**
 * @brief Stream status.
 * @see senscord::StreamState
 */
enum senscord_stream_state_t {
  SENSCORD_STREAM_STATE_UNDEFINED = 0,  /**< Undefined state */
  SENSCORD_STREAM_STATE_READY,          /**< Opened but not start */
  SENSCORD_STREAM_STATE_RUNNING,        /**< Started */
};

/**
 * @brief Property for the current state of the stream.
 * @see senscord::StreamStateProperty
 */
struct senscord_stream_state_property_t {
  enum senscord_stream_state_t state;
};

/**
 * FrameBufferingProperty
 * @see senscord::kFrameBufferingPropertyKey
 */
#define SENSCORD_FRAME_BUFFERING_PROPERTY_KEY   "frame_buffering_property"

/**
 * @brief Frame buffering.
 * @see senscord::Buffering
 */
enum senscord_buffering_t {
  SENSCORD_BUFFERING_USE_CONFIG = -2,   /**< Use config. */
  SENSCORD_BUFFERING_DEFAULT = -1,      /**< Buffering default. */
  SENSCORD_BUFFERING_OFF = 0,           /**< Buffering disable. */
  SENSCORD_BUFFERING_ON,                /**< Buffering enable. */
};

/**
 * @brief Frame buffering format.
 * @see senscord::BufferingFormat
 */
enum senscord_buffering_format_t {
  SENSCORD_BUFFERING_FORMAT_USE_CONFIG = -2,  /**< Use config. */
  SENSCORD_BUFFERING_FORMAT_DEFAULT = -1,   /**< Default format. */
  SENSCORD_BUFFERING_FORMAT_DISCARD,        /**< Discard the latest frame. */
  SENSCORD_BUFFERING_FORMAT_OVERWRITE,      /**< Overwrite the oldest frame. */
  /** @deprecated "queue" has been replaced by "discard". */
  SENSCORD_BUFFERING_FORMAT_QUEUE = SENSCORD_BUFFERING_FORMAT_DISCARD,
  /** @deprecated "ring" has been replaced by "overwrite". */
  SENSCORD_BUFFERING_FORMAT_RING = SENSCORD_BUFFERING_FORMAT_OVERWRITE
};

/**
 * Frame buffering number use config
 * @see senscord::kBufferNumUseConfig
 */
#define SENSCORD_BUFFER_NUM_USE_CONFIG     ((int32_t)-2)

/**
 * Frame buffering number default
 * @see senscord::kBufferNumDefault
 */
#define SENSCORD_BUFFER_NUM_DEFAULT     ((int32_t)-1)

/**
 * Frame buffering number default
 * @see senscord::kBufferNumUnlimited
 */
#define SENSCORD_BUFFER_NUM_UNLIMITED     (0)

/**
 * @brief Frame buffering setting.
 * @see senscord::FrameBufferingProperty
 */
struct senscord_frame_buffering_property_t {
  enum senscord_buffering_t buffering;      /**< Buffering enabling. */
  int32_t num;                              /**< Max buffering frame number, */
  enum senscord_buffering_format_t format;  /**< Buffering format. */
};

/**
 * CurrentFrameNumProperty
 * @see senscord::kCurrentFrameNumPropertyKey
 */
#define SENSCORD_CURRENT_FRAME_NUM_PROPERTY_KEY "current_frame_num_property"

/**
 * @brief Property for the current buffering frames.
 * @see senscord::CurrentFrameNumProperty
 */
struct senscord_current_frame_num_property_t {
  int32_t arrived_number;   /**< Arrived number. */
  int32_t received_number;  /**< Received number. */
};

/**
 * @brief Channel information.
 * @see senscord::ChannelInfo
 */
struct senscord_channel_info_t {
  /** Channel ID. */
  uint32_t channel_id;
  /** Type of raw data. */
  char raw_data_type[SENSCORD_RAWDATA_TYPE_LENGTH];
  /** Channel description. */
  char description[SENSCORD_CHANNEL_DESCRIPTION_LENGTH];
};

/**
 * ChannelInfoProperty
 * @see senscord::kChannelInfoPropertyKey
 */
#define SENSCORD_CHANNEL_INFO_PROPERTY_KEY "channel_info_property"

/**
 * @brief Property for channel information.
 * @see senscord::ChannelInfoProperty
 */
struct senscord_channel_info_property_t {
  /** Count of the array. */
  uint32_t count;
  /** Array of the channel information. */
  struct senscord_channel_info_t channels[SENSCORD_CHANNEL_LIST_MAX];
};


/**
 * ChannelMaskProperty
 * @see senscord::kChannelMaskPropertyKey
 */
#define SENSCORD_CHANNEL_MASK_PROPERTY_KEY "channel_mask_property"

/**
 * @brief Property for masking the channel.
 * @see senscord::ChannelMaskProperty
 */
struct senscord_channel_mask_property_t {
  /** Count of the array. */
  uint32_t count;
  /** Array of masked channel IDs. */
  uint32_t channels[SENSCORD_CHANNEL_LIST_MAX];
};

/**
 * @brief Property key definition for user data.
 *
 * This is the definition for comparing property keys,
 * and the following API does not use property keys.
 *
 * @see senscord_stream_get_userdata_property
 * @see senscord_stream_set_userdata_property
 *
 * @see senscord::kUserDataPropertyKey
 */
#define SENSCORD_USER_DATA_PROPERTY_KEY "user_data_property"

/**
 * @brief Recorder format.
 */
struct senscord_recoder_format_t {
  /** Format name */
  char name[SENSCORD_RECORDER_FORMAT_LENGTH];
};

/**
 * @brief Information about the record for each channel.
 */
struct senscord_record_info_t {
  /** Channel ID. */
  uint32_t channel_id;

  /**
   * Recorder format of each channel ID.
   * For get the available formats, use senscord_recorder_list_property.
   */
  struct senscord_recoder_format_t format;
};

/**
 * @brief Name rule information.
 */
struct senscord_record_name_rules_t {
  /** Directory type. */
  char directory_type[SENSCORD_RECORD_DIRECTORY_TYPE_LENGTH];
  /** Naming rule format string. */
  char format[SENSCORD_RECORD_NAME_RULE_LENGTH];
};

/**
 * RecordProperty
 * @see senscord::kRecordPropertyKey
 */
#define SENSCORD_RECORD_PROPERTY_KEY "record_property"

/**
 * @brief Property for the recording frames.
 * @see senscord::RecordProperty
 */
struct senscord_record_property_t {
  /**
   * State of recording.
   * If set to other than '0', recording will start.
   * Startable only in the stream running state.
   */
  uint8_t enabled;

  /**
   * Top directory path of recording files.
   * When to stop, this member is ignored.
   */
  char path[SENSCORD_FILE_PATH_LENGTH];

  /**
   * The count of record frames.
   */
  uint32_t count;

  /**
   * Count of the info_array.
   * When to stop, this member is ignored.
   */
  uint32_t info_count;

  /**
   * Array of the information about records each channel.
   * Frames of no specified channel ID will not be recorded.
   * When to stop, this member is ignored.
   */
  struct senscord_record_info_t info_array[SENSCORD_CHANNEL_LIST_MAX];

  /**
   * Number of the buffering of recording frame queue.
   * If set zero means the number equals one.
   * When to stop, this member is ignored.
   */
  uint32_t buffer_num;

  /**
   * Count of the name_rules.
   * When to stop, this member is ignored.
   */
  uint32_t name_rules_count;

  /**
   * Directory naming rules.
   * When to stop, this member is ignored.
   */
  struct senscord_record_name_rules_t
      name_rules[SENSCORD_RECORD_NAME_RULE_LIST_MAX];
};

// Standard recording formats.
#define SENSCORD_RECORDING_FORMAT_RAW "raw"
#define SENSCORD_RECORDING_FORMAT_COMPOSITE_RAW "composite_raw"
#define SENSCORD_RECORDING_FORMAT_SKV "skv"

// Directory types.
#define SENSCORD_RECORD_DIRECTORY_TOP "top"

/**
 * RecorderListProperty
 * @see senscord::kRecorderListPropertyKey
 */
#define SENSCORD_RECORDER_LIST_PROPERTY_KEY "recorder_list_property"

/**
 * @brief Property for reference the available recording formats.
 *        For stream property getting only.
 * @see senscord::RecorderListProperty
 */
struct senscord_recorder_list_property_t {
  /** Count of the recorder formats. */
  uint32_t count;
  /** Array of recorder format. */
  struct senscord_recoder_format_t formats[SENSCORD_RECORDER_FORMAT_LIST_MAX];
};

/**
 * PlayModeProperty
 * @see senscord::kPlayModePropertyKey
 */
#define SENSCORD_PLAY_MODE_PROPERTY_KEY "play_mode_property"

/**
 * @brief Property for the mode of replay the stream.
 * @see senscord::PlayModeProperty
 */
struct senscord_play_mode_property_t {
  /** Enabling the repeat play. (0:disable, 1:enable) */
  uint8_t repeat;
};

/**
 * PlayPauseProperty
 * @see senscord::kPlayPausePropertyKey
 */
#define SENSCORD_PLAY_PAUSE_PROPERTY_KEY "play_pause_property"

/**
 * @brief Property for the playback pause mode of the stream.
 * @see senscord::PlayPauseProperty
 */
struct senscord_play_pause_property_t {
  /** Enabling the pause mode. (0:disable, 1:enable) */
  uint8_t pause;
};

/**
 * @brief Frame replay speed with player.
 * @see senscord::PlaySpeed
 */
enum senscord_play_speed_t {
  /** Sending based on framerate. */
  SENSCORD_PLAY_SPEED_BASED_ON_FRAMERATE = 0,
  /** @deprecated "BestEffort" replaces "FrameRate" in player component */
  SENSCORD_PLAY_SPEED_BEST_EFFORT
};

/**
 * PlayProperty
 * @see senscord::kPlayPropertyKey
 */
#define SENSCORD_PLAY_PROPERTY_KEY "play_property"

/**
 * @brief Property for the settings before replay stream.
 * @see senscord::PlayProperty
 */
struct senscord_play_property_t {
  /** Directory path of recorded. */
  char target_path[SENSCORD_FILE_PATH_LENGTH];
  /** Offset of starting frame. */
  uint32_t start_offset;
  /** Playing frames from start_offset. */
  uint32_t count;
  /** Play speed. */
  enum senscord_play_speed_t speed;
  /** Play mode. */
  struct senscord_play_mode_property_t mode;
};

/**
 * play count for playing all frames from start_offset.
 * @see senscord::kPlayCountAll
 */
#define SENSCORD_PLAY_COUNT_ALL (0)

/**
 * PlayFileInfoProperty
 * @see senscord::kPlayFileInfoPropertyKey
 */
#define SENSCORD_PLAY_FILE_INFO_PROPERTY_KEY "play_file_info_property"

/**
 * @brief Property for playback file information.
 * @see senscord::PlayFileInfoProperty
 */
struct senscord_play_file_info_property_t {
  /** Directory path of recorded. */
  char target_path[SENSCORD_FILE_PATH_LENGTH];
  /** Recorded date and time. */
  char record_date[SENSCORD_RECORD_DATE_LENGTH];
  /** Stream key to recorded. */
  char stream_key[SENSCORD_STREAM_KEY_LENGTH];
  /** Stream type to recorded. */
  char stream_type[SENSCORD_STREAM_TYPE_LENGTH];
  /** Number of Frames recorded. */
  uint32_t frame_count;
};

/**
 * PlayPositionProperty
 * @see senscord::kPlayPositionPropertyKey
 */
#define SENSCORD_PLAY_POSITION_PROPERTY_KEY "play_position_property"

/**
 * @brief Property that indicates the playback position in the player function.
 * @see senscord::PlayPositionProperty
 */
struct senscord_play_position_property_t {
  uint32_t position;
};

/**
 * @brief Vector2. (float)
 * @see senscord::Vector2
 */
struct senscord_vector2f_t {
  float x;
  float y;
};

/**
 * @brief Vector3. (float)
 * @see senscord::Vector3
 */
struct senscord_vector3f_t {
  float x;
  float y;
  float z;
};

/**
 * @brief Vector2. (uint32_t)
 * @see senscord::Vector2
 */
struct senscord_vector2u32_t {
  uint32_t x;
  uint32_t y;
};

/**
 * @brief Vector3. (uint32_t)
 * @see senscord::Vector3
 */
struct senscord_vector3u32_t {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

/**
 * @brief Vector4.
 * @see senscord::Vector4
 */
struct senscord_vector4f_t {
  float x;
  float y;
  float z;
  float a;
};

/**
 * @brief Quaternion.
 * @see senscord::Quaternion
 */
struct senscord_quaternion_f_t {
  float x;
  float y;
  float z;
  float w;
};

/**
 * @brief 3x3 matrix.
 * @see senscord::Matrix3x3
 */
struct senscord_matrix3x3f_t {
  float element[3][3];
};

/**
 * @brief 3x4 matrix.
 * @see senscord::Matrix3x4
 */
struct senscord_matrix3x4f_t {
  float element[3][4];
};

/**
 * @brief Scalar. (uint32_t)
 * @see senscord::Scalar
 */
struct senscord_scalar_u32_t {
  uint32_t value;
};

/**
 * @brief Scalar. (float)
 * @see senscord::Scalar
 */
struct senscord_scalar_f_t {
  float value;
};

/**
 * @brief Range expressed by the min max. (uint32_t)
 * @see senscord::Range
 */
struct senscord_range_u32_t {
  uint32_t min;
  uint32_t max;
};

/**
 * @brief Misalignment of the axis direction.
 * @see senscord::AxisMisalignment
 */
struct senscord_axis_misalignment_t {
  struct senscord_matrix3x3f_t ms;
  struct senscord_vector3f_t offset;
};

/**
 * @brief An enumerated type representing a unit of length.
 * @see senscord::UnitLength
 */
enum senscord_unit_length_t {
  SENSCORD_METRE,      /**< Unit:[m]. */
  SENSCORD_MILLIMETRE  /**< Unit:[mm]. */
};

/**
 * @brief Units used for acceleration.
 * @see senscord::AccelerationUnit
 */
enum senscord_acceleration_unit_t {
  SENSCORD_ACCELERATION_NOT_SUPPORTED,  /**< sensor not supported. */
  SENSCORD_GRAVITATIONAL,            /**< Unit:[G]. */
  SENSCORD_METRE_PER_SECOND_SQUARED  /**< Unit:[m/s2]. */
};

/**
 * @brief Units used for angular velocity.
 * @see senscord::AngularVelocityUnit
 */
enum senscord_angular_velocity_unit_t {
  SENSCORD_ANGULAR_VELOCITY_NOT_SUPPORTED,  /**< sensor not supported. */
  SENSCORD_DEGREE_PER_SECOND,  /**< Unit:[deg/s]. */
  SENSCORD_RADIAN_PER_SECOND   /**< Unit:[rad/s]. */
};

/**
 * @brief Units used for magnetic field.
 * @see senscord::MagneticFieldUnit
 */
enum senscord_magnetic_field_unit_t {
  SENSCORD_MAGNETIC_FIELD_NOT_SUPPORTED,  /**< sensor not supported. */
  SENSCORD_GAUSS,       /**< Unit:[gauss]. */
  SENSCORD_MICRO_TESLA  /**< Unit:[uT]. */
};

/**
 * @brief Units used for orientataion.
 * @see senscord::OrientationUnit
 */
enum senscord_orientation_unit_t {
  SENSCORD_ORIENTATION_NOT_SUPPORTED,  /**< sensor not supported. */
  SENSCORD_DEGREE,  /**< Unit:[deg]. */
  SENSCORD_RADIAN   /**< Unit:[rad]. */
};

/**
 * @brief Types of coordinate system.
 * @see senscord::CoordinateSystem
 */
enum senscord_coordinate_system_t {
  SENSCORD_WORLD_COORDINATE,  /**< World coordinate system. */
  SENSCORD_LOCAL_COORDINATE,  /**< Local coordinate system. */
  SENSCORD_CAMERA_COORDINATE  /**< Camera coordinate system. */
};

/**
 * @brief Units of grid.
 * @see senscord::GridUnit
 */
enum senscord_grid_unit_t {
  SENSCORD_GRID_PIXEL,        /**< Unit:[pixel]. */
  SENSCORD_GRID_METER         /**< Unit:[m]. */
};

/**
 * @brief The field types of interlace.
 * @see senscord::InterlaceField
 */
enum senscord_interlace_field_t {
  SENSCORD_INTERLACE_FIELD_TOP,     /**< Top field */
  SENSCORD_INTERLACE_FIELD_BOTTOM   /**< Bottom field */
};

/**
 * @brief The order of interlace.
 * @see senscord::InterlaceOrder
 */
enum senscord_interlace_order_t {
  SENSCORD_INTERLACE_ORDER_TOP_FIRST,       /**< Top first */
  SENSCORD_INTERLACE_ORDER_BOTTOM_FIRST     /**< Bottom first */
};

/**
 * @brief Units used for velocity.
 * @see senscord::VelocityUnit
 */
enum senscord_velocity_unit_t {
  SENSCORD_VELOCITY_NOT_SUPPORTED,  /**< not supported. */
  SENSCORD_METRE_PER_SECOND,  /**< Unit:[m/s]. */
  SENSCORD_PIXEL_PER_SECOND   /**< Unit:[pixel/s]. */
};

/**
 * @brief System handed for CoordinateSystem.
 */
enum senscord_system_handed_t {
  SENSCORD_SYSTEM_HANDED_LEFT,       /**< Left-handed system. */
  SENSCORD_SYSTEM_HANDED_RIGHT,      /**< Right-handed system. */
};

/**
 * @brief Up axis for CoordinateSystem.
 */
enum senscord_up_axis_t {
  SENSCORD_UP_AXIS_UNDEFINED,       /**< UpAxis undefined. */
  SENSCORD_UP_AXIS_PLUS_X,          /**< X axis up. */
  SENSCORD_UP_AXIS_PLUS_Y,          /**< Y axis up. */
  SENSCORD_UP_AXIS_PLUS_Z,          /**< Z axis up. */
  SENSCORD_UP_AXIS_MINUS_X,         /**< X axis down. */
  SENSCORD_UP_AXIS_MINUS_Y,         /**< Y axis down. */
  SENSCORD_UP_AXIS_MINUS_Z,         /**< Z axis down. */
};

/**
 * @brief Forward axis for CoordinateSystem.
 */
enum senscord_forward_axis_t {
  SENSCORD_FORWARD_AXIS_UNDEFINED,       /**< Forward undefined. */
  SENSCORD_FORWARD_AXIS_PLUS_X,          /**< X axis up. */
  SENSCORD_FORWARD_AXIS_PLUS_Y,          /**< Y axis up. */
  SENSCORD_FORWARD_AXIS_PLUS_Z,          /**< Z axis up. */
  SENSCORD_FORWARD_AXIS_MINUS_X,         /**< X axis down. */
  SENSCORD_FORWARD_AXIS_MINUS_Y,         /**< Y axis down. */
  SENSCORD_FORWARD_AXIS_MINUS_Z,         /**< Z axis down. */
};

/**
 * @brief Structure that represents the preset information.
 */
struct senscord_preset_info_t {
  /** Preset ID */
  uint32_t id;
  /** Description of the preset. */
  char description[SENSCORD_PRESET_DESCRIPTION_LENGTH];
};

/**
 * PresetListProperty
 * @see senscord::kPresetListPropertyKey
 */
#define SENSCORD_PRESET_LIST_PROPERTY_KEY "preset_list_property"

/**
 * @brief Property for the list of property's preset IDs.
 * @see senscord::PresetListProperty
 */
struct senscord_preset_list_property_t {
  /** Count of the array. */
  uint32_t count;
  /** Array of the preset information. */
  struct senscord_preset_info_t presets[SENSCORD_PRESET_LIST_MAX];
};

/**
 * PresetProperty
 * @see senscord::kPresetPropertyKey
 */
#define SENSCORD_PRESET_PROPERTY_KEY "preset_property"

/**
 * @brief Property for the property's preset.
 * @see senscord::PresetProperty
 */
struct senscord_preset_property_t {
  uint32_t id;  /**< Preset ID */
};

/**
 * ImageProperty
 * @see senscord::kImagePropertyKey
 */
#define SENSCORD_IMAGE_PROPERTY_KEY     "image_property"

/**
 * @brief Structures that handle properties of Raw data of Image and Depth data.
 * @see senscord::ImageProperty
 */
struct senscord_image_property_t {
  uint32_t width;         /**< Image width. */
  uint32_t height;        /**< Image height. */
  uint32_t stride_bytes;  /**< Image stride. */
  /** The format of a pixel. */
  char pixel_format[SENSCORD_PIXEL_FORMAT_LENGTH];
};

/**
 * ConfidenceProperty
 * @see senscord::kConfidencePropertyKey
 */
#define SENSCORD_CONFIDENCE_PROPERTY_KEY "confidence_property"

/**
 * @brief Structures that handle properties of raw data of confidence.
 * @see senscord::ConfidenceProperty
 */
struct senscord_confidence_property_t {
  uint32_t width;         /**< Image width. */
  uint32_t height;        /**< Image height. */
  uint32_t stride_bytes;  /**< Image stride. */
  /** The format of a pixel. */
  char pixel_format[SENSCORD_PIXEL_FORMAT_LENGTH];
};

/**
 * @brief Pixel formats
 */
/* Packed RGB */
#define SENSCORD_PIXEL_FORMAT_ARGB444 "image_argb444"   /* ARGB 4444 */
#define SENSCORD_PIXEL_FORMAT_XRGB444 "image_xrgb444"   /* XRGB 4444 */
#define SENSCORD_PIXEL_FORMAT_RGB24   "image_rgb24"     /* RGB 888 */
#define SENSCORD_PIXEL_FORMAT_ARGB32  "image_argb32"    /* ARGB 8888 */
#define SENSCORD_PIXEL_FORMAT_XRGB32  "image_xrgb32"    /* XRGB 8888 */
#define SENSCORD_PIXEL_FORMAT_BGR24   "image_bgr24"     /* BGR 888 */
#define SENSCORD_PIXEL_FORMAT_ABGR32  "image_abgr32"    /* ABGR 8888 */
#define SENSCORD_PIXEL_FORMAT_XBGR32  "image_xbgr32"    /* XBGR 8888 */

/* Planar RGB */
/* RGB 8-bit */
#define SENSCORD_PIXEL_FORMAT_RGB8_PLANAR   "image_rgb8_planar"
/* RGB 16-bit */
#define SENSCORD_PIXEL_FORMAT_RGB16_PLANAR  "image_rgb16_planar"

/* Greyscale */
/* 8-bit Greyscale */
#define SENSCORD_PIXEL_FORMAT_GREY    "image_grey"
/* 10-bit Greyscale (on 16bit) */
#define SENSCORD_PIXEL_FORMAT_Y10     "image_y10"
/* 12-bit Greyscale (on 16bit) */
#define SENSCORD_PIXEL_FORMAT_Y12     "image_y12"
/* 14-bit Greyscale (on 16bit) */
#define SENSCORD_PIXEL_FORMAT_Y14     "image_y14"
/* 16-bit Greyscale */
#define SENSCORD_PIXEL_FORMAT_Y16     "image_y16"
/* 20-bit Greyscale (on 32bit) */
#define SENSCORD_PIXEL_FORMAT_Y20     "image_y20"
/* 24-bit Greyscale (on 32bit) */
#define SENSCORD_PIXEL_FORMAT_Y24     "image_y24"

/* YUV */
#define SENSCORD_PIXEL_FORMAT_YUV444  "image_yuv444"    /* YUV444 */
#define SENSCORD_PIXEL_FORMAT_NV12    "image_nv12"      /* YUV420SP */
#define SENSCORD_PIXEL_FORMAT_NV16    "image_nv16"      /* YUV422SP */
#define SENSCORD_PIXEL_FORMAT_YUV420  "image_yuv420"    /* YUV420 */
#define SENSCORD_PIXEL_FORMAT_YUV422P "image_yuv422p"   /* YUV422P */
#define SENSCORD_PIXEL_FORMAT_YUYV    "image_yuyv"      /* YUYV */
#define SENSCORD_PIXEL_FORMAT_UYVY    "image_uyvy"      /* UYVY */

/* Bayer */
#define SENSCORD_PIXEL_FORMAT_SBGGR8  "image_sbggr8"
#define SENSCORD_PIXEL_FORMAT_SGBRG8  "image_sgbrg8"
#define SENSCORD_PIXEL_FORMAT_SGRBG8  "image_sgrbg8"
#define SENSCORD_PIXEL_FORMAT_SRGGB8  "image_srggb8"

#define SENSCORD_PIXEL_FORMAT_SBGGR10 "image_sbggr10"
#define SENSCORD_PIXEL_FORMAT_SGBRG10 "image_sgbrg10"
#define SENSCORD_PIXEL_FORMAT_SGRBG10 "image_sgrbg10"
#define SENSCORD_PIXEL_FORMAT_SRGGB10 "image_srggb10"

#define SENSCORD_PIXEL_FORMAT_SBGGR12 "image_sbggr12"
#define SENSCORD_PIXEL_FORMAT_SGBRG12 "image_sgbrg12"
#define SENSCORD_PIXEL_FORMAT_SGRBG12 "image_sgrbg12"
#define SENSCORD_PIXEL_FORMAT_SRGGB12 "image_srggb12"

/* Quad Bayer */
#define SENSCORD_PIXEL_FORMAT_QUAD_SBGGR8  "image_quad_sbggr8"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGBRG8  "image_quad_sgbrg8"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGRBG8  "image_quad_sgrbg8"
#define SENSCORD_PIXEL_FORMAT_QUAD_SRGGB8  "image_quad_srggb8"

#define SENSCORD_PIXEL_FORMAT_QUAD_SBGGR10 "image_quad_sbggr10"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGBRG10 "image_quad_sgbrg10"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGRBG10 "image_quad_sgrbg10"
#define SENSCORD_PIXEL_FORMAT_QUAD_SRGGB10 "image_quad_srggb10"

#define SENSCORD_PIXEL_FORMAT_QUAD_SBGGR12 "image_quad_sbggr12"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGBRG12 "image_quad_sgbrg12"
#define SENSCORD_PIXEL_FORMAT_QUAD_SGRBG12 "image_quad_sgrbg12"
#define SENSCORD_PIXEL_FORMAT_QUAD_SRGGB12 "image_quad_srggb12"

/* Polarization image */
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_Y8 \
    "image_polar_90_45_135_0_y8"
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_Y10 \
    "image_polar_90_45_135_0_y10"
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_Y12 \
    "image_polar_90_45_135_0_y12"
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_RGGB8 \
    "image_polar_90_45_135_0_rggb8"
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_RGGB10 \
    "image_polar_90_45_135_0_rggb10"
#define SENSCORD_PIXEL_FORMAT_POLAR_90_45_135_0_RGGB12 \
    "image_polar_90_45_135_0_rggb12"

/* Compressed image */
#define SENSCORD_PIXEL_FORMAT_JPEG    "image_jpeg"
#define SENSCORD_PIXEL_FORMAT_H264    "image_h264"

/* Depth */
#define SENSCORD_PIXEL_FORMAT_Z16     "depth_z16"  /* 16-bit Z-Depth */
#define SENSCORD_PIXEL_FORMAT_Z32F    "depth_z32f" /* 32-bit float Z-Depth */
#define SENSCORD_PIXEL_FORMAT_D16     "depth_d16"  /* 16-bit Disparity */

/* Confidence */
/* 1-bit positive confidence */
#define SENSCORD_PIXEL_FORMAT_C1P     "confidence_c1p"
/* 1-bit negative confidence */
#define SENSCORD_PIXEL_FORMAT_C1N     "confidence_c1n"
/* 16-bit confidence */
#define SENSCORD_PIXEL_FORMAT_C16     "confidence_c16"
/* 32-bit float confidence */
#define SENSCORD_PIXEL_FORMAT_C32F    "confidence_c32f"

/* PointCloud */
/* signed   16-bit (x, y, depth) */
#define SENSCORD_PIXEL_FORMAT_XYZ16         "point_cloud_xyz16"
/* signed   16-bit (x, y, depth, rgb) */
#define SENSCORD_PIXEL_FORMAT_XYZRGB16      "point_cloud_xyzrgb16"
/* signed   32-bit (x, y, depth) */
#define SENSCORD_PIXEL_FORMAT_XYZ32         "point_cloud_xyz32"
/* signed   32-bit (x, y, depth, rgb) */
#define SENSCORD_PIXEL_FORMAT_XYZRGB32      "point_cloud_xyzrgb32"
/* unsigned 16-bit (x, y, depth) */
#define SENSCORD_PIXEL_FORMAT_XYZ16U        "point_cloud_xyz16u"
/* unsigned 16-bit (x, y, depth, rgb) */
#define SENSCORD_PIXEL_FORMAT_XYZRGB16U     "point_cloud_xyzrgb16u"
/* unsigned 32-bit (x, y, depth) */
#define SENSCORD_PIXEL_FORMAT_XYZ32U        "point_cloud_xyz32u"
/* unsigned 32-bit (x, y, depth, rgb) */
#define SENSCORD_PIXEL_FORMAT_XYZRGB32U     "point_cloud_xyzrgb32u"
/* signed   32-bit float (x, y, depth) */
#define SENSCORD_PIXEL_FORMAT_XYZ32F        "point_cloud_xyz32f"
/* signed   32-bit float (x, y, depth, rgb) */
#define SENSCORD_PIXEL_FORMAT_XYZRGB32F     "point_cloud_xyzrgb32f"
/* signed   16-bit (x, y, depth) planar array */
#define SENSCORD_PIXEL_FORMAT_XYZ16_PLANAR  "point_cloud_xyz16_planar"
/* unsigned 16-bit (x, y, depth) planar array */
#define SENSCORD_PIXEL_FORMAT_XYZ16U_PLANAR "point_cloud_xyz16u_planar"
/* signed   32-bit float(x, y, depth) planar array */
#define SENSCORD_PIXEL_FORMAT_XYZ32F_PLANAR "point_cloud_xyz32f_planar"

/* GridMap */
/* 1-bit positive voxel data */
#define SENSCORD_PIXEL_FORMAT_GRID_MAP_1P1N = "grid_map_1p1n"

/**
 * @brief Encoding types for YUV (YCbCr).
 * @see senscord::YCbCrEncoding
 */
enum senscord_ycbcr_encoding_t {
  SENSCORD_YCBCR_ENCODING_UNDEFINED,
  SENSCORD_YCBCR_ENCODING_BT601,
  SENSCORD_YCBCR_ENCODING_BT709,
  SENSCORD_YCBCR_ENCODING_BT2020,
  SENSCORD_YCBCR_ENCODING_BT2100
};

/**
 * @brief Quantization types for YUV (YCbCr).
 * @see senscord::YCbCrQuantization
 */
enum senscord_ycbcr_quantization_t {
  SENSCORD_YCBCR_QUANTIZATION_UNDEFINED,
  SENSCORD_YCBCR_QUANTIZATION_FULL_RANGE,     /* Y: 0-255,  C: 0-255 */
  SENSCORD_YCBCR_QUANTIZATION_LIMITED_RANGE,  /* Y: 16-235, C: 16-240 */
  SENSCORD_YCBCR_QUANTIZATION_SUPER_WHITE
};

/**
 * ColorSpaceProperty
 * @see senscord::kColorSpacePropertyKey
 */
#define SENSCORD_COLOR_SPACE_PROPERTY_KEY "color_space_property"

/**
 * @brief Property of color space type for YUV.
 * @see senscord::ColorSpaceProperty
 */
struct senscord_color_space_property_t {
  enum senscord_ycbcr_encoding_t encoding;
  enum senscord_ycbcr_quantization_t quantization;
};

/**
 * FrameRateProperty
 * @see senscord::kFrameRatePropertyKey
 */
#define SENSCORD_FRAME_RATE_PROPERTY_KEY        "frame_rate_property"

/**
 * @brief Structure for setting frame rate.
 *
 * Specify in the style of numerator / denominator.
 * ex) 60fps : num = 60, denom = 1
 * @see senscord::FrameRateProperty
 */
struct senscord_frame_rate_property_t {
  uint32_t num;    /**< Framerate numerator. */
  uint32_t denom;  /**< Framerate denominator. */
};

/**
 * SkipFrameProperty
 * @see senscord::kSkipFramePropertyKey
 */
#define SENSCORD_SKIP_FRAME_PROPERTY_KEY "skip_frame_property"

/**
 * @brief Structure for setting the skip rate of the frame.
 *
 * If 'rate = 1' is specified, frames are not skipped.
 * If 'rate = N' (N is 2 or more) is specified, the frame is skipped and
 * the frame rate drops to 1 / N.
 * @see senscord::SkipFrameProperty
 */
struct senscord_skip_frame_property_t {
  uint32_t rate;
};

/**
 * LensProperty
 * @see senscord::kLensPropertyKey
 */
#define SENSCORD_LENS_PROPERTY_KEY      "lens_property"

/**
 * @brief Structure used to acquire field angle of camera.
 * @see senscord::LensProperty
 */
struct senscord_lens_property_t {
  /** The horizontal viewing angle of the lens. */
  float horizontal_field_of_view;
  /** The vertical viewing angle of the lens. */
  float vertical_field_of_view;
};

/**
 * DepthProperty
 * @see senscord::kDepthPropertyKey
 */
#define SENSCORD_DEPTH_PROPERTY_KEY     "depth_property"

/**
 * @brief Structure for handling Depth data properties.
 * @see senscord::DepthProperty
 */
struct senscord_depth_property_t {
  /**
   * Scale of the depth value, in metres.
   * By multiplying this value, the depth value is converted to metres.
   */
  float scale;
  float depth_min_range;   /**< Minimum depth value of the sensor. */
  float depth_max_range;   /**< Maximum depth value of the sensor. */
};

/**
 * ImageSensorFunctionProperty
 * @see senscord::kImageSensorFunctionPropertyKey
 */
#define SENSCORD_IMAGE_SENSOR_FUNCTION_PROPERTY_KEY \
    "image_sensor_function_property"

/**
 * @brief Structures used to set the functions used in the sensor.
 * @see senscord::ImageSensorFunctionProperty
 */
struct senscord_image_sensor_function_property_t {
  /** Setting auto exposure function. */
  uint8_t auto_exposure;
  /** Setting automatic white balance function. */
  uint8_t auto_white_balance;
  int32_t brightness;       /**< Brightness value. */
  uint32_t iso_sensitivity; /**< ISO sensitivity. (100,200,400,800,1600,...) */
  uint32_t exposure_time;   /**< Time of exposure [100usec]. */
  /** Exposure metering mode. */
  char exposure_metering[SENSCORD_EXPOSURE_METERING_LENGTH];
  float gamma_value;        /**< Gamma correction value */
  uint32_t gain_value;      /**< Gain value. */
  int32_t hue;              /**< Hue value. */
  int32_t saturation;       /**< Saturation value. */
  int32_t sharpness;        /**< Sharpness value. */
  int32_t white_balance;    /**< White balance value. */
};

/**
 * @brief Fixed values for the exposure.
 */
/* Exposure time : Auto */
#define SENSCORD_EXPOSURE_TIME_AUTO (0)
/* ISO Sensitivity : Auto */
#define SENSCORD_ISO_SENSITIVITY_AUTO (0)

/**
 * @brief Exposure metering modes.
 */
/* Metering mode : None (Disable) */
#define SENSCORD_EXPOSURE_METERING_NONE "none"
/* Metering mode : Average */
#define SENSCORD_EXPOSURE_METERING_AVERAGE "average"
/* Metering mode : Center weighted */
#define SENSCORD_EXPOSURE_METERING_CENTER_WEIGHTED "center_weighted"
/* Metering mode : Spot */
#define SENSCORD_EXPOSURE_METERING_SPOT "spot"
/* Metering mode : Matrix */
#define SENSCORD_EXPOSURE_METERING_MATRIX "matrix"

/**
 * ImageSensorFunctionSupportedProperty
 * @see senscord::kImageSensorFunctionSupportedPropertyKey
 */
#define SENSCORD_IMAGE_SENSOR_FUNCTION_SUPPORTED_PROPERTY_KEY \
    "image_sensor_function_supported_property"

/**
 * @brief Structure for acquiring functions supported by Component.
 *
 * For each function of the image sensor, set the counter corresponding
 * to Componet as a boolean value.
 *
 * @see senscord::ImageSensorFunctionSupportedProperty
 */
struct senscord_image_sensor_function_supported_property_t {
  uint8_t auto_exposure_supported;
  uint8_t auto_white_balance_supported;
  uint8_t brightness_supported;
  uint8_t iso_sensitivity_supported;
  uint8_t exposure_time_supported;
  uint8_t exposure_metering_supported;
  uint8_t gamma_value_supported;
  uint8_t gain_value_supported;
  uint8_t hue_supported;
  uint8_t saturation_supported;
  uint8_t sharpness_supported;
  uint8_t white_balance_supported;
};

/**
 * @brief Structure for the region of plane for AE or ROI, etc.
 */
struct senscord_rectangle_region_parameter_t {
  // region of rectangle.
  uint32_t top;     /**< upper position of region from origin  */
  uint32_t left;    /**< left  position of region from origin  */
  uint32_t bottom;  /**< bottom position of region from origin */
  uint32_t right;   /**< right position of region from origin  */
};

/**
 * ExposureProperty
 * @see senscord::kExposurePropertyKey
 */
#define SENSCORD_EXPOSURE_PROPERTY_KEY "exposure_property"

/**
 * @brief Structure for the image of the camera exposure.
 * @see senscord::ExposureProperty
 */
struct senscord_exposure_property_t {
  char mode[SENSCORD_EXPOSURE_MODE_LENGTH];  /**< Mode of exposure. */
  float ev_compensation;      /**< Compensation value of EV. */
  uint32_t exposure_time;     /**< Time of exposure. [usec] */
  uint32_t iso_sensitivity;   /**< ISO sensitivity. (100,200,400,800 ...) */
  /** Exposure metering mode. */
  char metering[SENSCORD_EXPOSURE_METERING_LENGTH];
  /** Target region of the camera exposure. */
  struct senscord_rectangle_region_parameter_t target_region;
};

/**
 * @brief Exposure modes.
 */
/* Exposure mode : Auto */
#define SENSCORD_EXPOSURE_MODE_AUTO "auto"
/* Exposure mode : Hold */
#define SENSCORD_EXPOSURE_MODE_HOLD "hold"
/* Exposure mode : Manual */
#define SENSCORD_EXPOSURE_MODE_MANUAL "manual"
/* Exposure mode : Gain Fix */
#define SENSCORD_EXPOSURE_MODE_GAIN_FIX "gainfix"
/* Exposure mode : Time Fix */
#define SENSCORD_EXPOSURE_MODE_TIME_FIX "timefix"

/**
 * WhiteBalanceProperty
 * @see senscord::kWhiteBalancePropertyKey
 */
#define SENSCORD_WHITE_BALANCE_PROPERTY_KEY "white_balance_property"

/**
 * @brief Structure for the white balance.
 * @see senscord::WhiteBalanceProperty
 */
struct senscord_white_balance_property_t {
  /** Mode of white balance. */
  char mode[SENSCORD_WHITE_BALANCE_MODE_LENGTH];
};

/**
 * @brief White balance modes.
 */
/* White balance mode : Auto */
#define SENSCORD_WHITE_BALANCE_MODE_AUTO "auto"
/* White balance mode : Manual */
#define SENSCORD_WHITE_BALANCE_MODE_MANUAL "manual"

/**
 * @brief Structure for handling internal parameters of calibration.
 * @see senscord::IntrinsicCalibrationParameter
 */
struct senscord_intrinsic_calibration_parameter_t {
  float cx;  /**< The x-axis coordinate of the optical center point. */
  float cy;  /**< The y-axis coordinate of the optical center point. */
  float fx;  /**< Focal length on x axis. */
  float fy;  /**< Focal length on y axis. */
  float s;   /**< skewness. */
};

/**
 * @brief Structure for handling extrinsic parameters of calibration.
 * @see senscord::ExtrinsicCalibrationParameter
 */
struct senscord_extrinsic_calibration_parameter_t {
  float r11;  /**< Extrinsic parameter r11. */
  float r12;  /**< Extrinsic parameter r12. */
  float r13;  /**< Extrinsic parameter r13. */
  float r21;  /**< Extrinsic parameter r21. */
  float r22;  /**< Extrinsic parameter r22. */
  float r23;  /**< Extrinsic parameter r23. */
  float r31;  /**< Extrinsic parameter r31. */
  float r32;  /**< Extrinsic parameter r32. */
  float r33;  /**< Extrinsic parameter r33. */
  float t1;   /**< Extrinsic parameter t1. */
  float t2;   /**< Extrinsic parameter t2. */
  float t3;   /**< Extrinsic parameter t3. */
  struct senscord_matrix3x4f_t p;   /**< Extrinsic parameter p11-p34. */
};

/**
 * @brief Structure for handling camera distortion coefficient.
 * @see senscord::DistortionCalibrationParameter
 */
struct senscord_distortion_calibration_parameter_t {
  float k1;  /**< Camera distortion coefficient k1. */
  float k2;  /**< Camera distortion coefficient k2. */
  float k3;  /**< Camera distortion coefficient k3. */
  float k4;  /**< Camera distortion coefficient k4. */
  float k5;  /**< Camera distortion coefficient k5. */
  float k6;  /**< Camera distortion coefficient k6. */
  float p1;  /**< Camera distortion coefficient p1. */
  float p2;  /**< Camera distortion coefficient p1. */
};

/**
 * @brief Calibration parameters of a single camera.
 * @see senscord::CameraCalibrationParameters
 */
struct senscord_camera_calibration_parameters_t {
  uint32_t channel_id;
  /** Camera internal parameters. */
  struct senscord_intrinsic_calibration_parameter_t intrinsic;
  /** Distortion correction coefficient. */
  struct senscord_distortion_calibration_parameter_t distortion;
  /** Extrinsic parameters. */
  struct senscord_extrinsic_calibration_parameter_t extrinsic;
};

/**
 * CameraCalibrationProperty
 * @see senscord::kCameraCalibrationPropertyKey
 */
#define SENSCORD_CAMERA_CALIBRATION_PROPERTY_KEY "camera_calibration_property"

/**
 * @brief Property for camera calibration.
 * @see senscord::CameraCalibrationProperty
 */
struct senscord_camera_calibration_property_t {
  /** Count of the array. */
  uint32_t count;
  /** Array of the camera calibration parameters. */
  struct senscord_camera_calibration_parameters_t
      parameters[SENSCORD_CHANNEL_LIST_MAX];
};

/**
 * InterlaceProperty
 * @see senscord::kInterlacePropertyKey
 */
#define SENSCORD_INTERLACE_PROPERTY_KEY         "interlace_property"

/**
 * @brief Channel's property for interlace.
 * @see senscord::InterlaceProperty
 */
struct senscord_interlace_property_t {
  enum senscord_interlace_field_t field;      /**< contained field type. */
};

/**
 * InterlaceInfoProperty
 * @see senscord::kInterlaceInfoProperty
 */
#define SENSCORD_INTERLACE_INFO_PROPERTY_KEY    "interlace_info_property"

/**
 * @brief Property for interlace informations.
 * @see senscord::InterlaceInfoProperty
 */
struct senscord_interlace_info_property_t {
  enum senscord_interlace_order_t order;        /**< order of field */
};

/**
 * ImageCropProperty
 * @see senscord::kImageCropPropertyKey
 */
#define SENSCORD_IMAGE_CROP_PROPERTY_KEY        "image_crop_property"

/**
 * @brief Property for image cropping.
 * @see senscord::ImageCropProperty
 */
struct senscord_image_crop_property_t {
  /** Horizontal offset of the top left corner of the cropping rectangle. */
  uint32_t left;
  /** Vertical offset of the top left corner of the cropping rectangle. */
  uint32_t top;
  uint32_t width;   /**< Width of the cropping rectangle. */
  uint32_t height;  /**< Height of the cropping rectangle. */
};

/**
 * ImageCropBoundsProperty
 * @see senscord::kImageCropBoundsPropertyKey
 */
#define SENSCORD_IMAGE_CROP_BOUNDS_PROPERTY_KEY   "image_crop_bounds_property"

/**
 * @brief This Property is there to represent the bounds of the crop.
 * @see senscord::ImageCropBoundsProperty
 */
struct senscord_image_crop_bounds_property_t {
  /** Horizontal offset of the cropable rectangle area. In pixels. */
  uint32_t left;
  /** Vertical offset of the cropable rectangle. In pixels. */
  uint32_t top;
  /** Width of the cropable rectangle from the offset. In pixels. */
  uint32_t width;
  /** Height of the cropable rectangle from the offset. In pixels. */
  uint32_t height;
};

/**
 * BaselineLengthProperty
 * @see senscord::kBaselineLengthPropertyKey
 */
#define SENSCORD_BASE_LINE_LENGTH_PROPERTY_KEY  "baseline_length_property"

/**
 * @brief Structure for handling baseline length between cameras.
 * @see senscord::BaselineLengthProperty
 */
struct senscord_base_line_length_property_t {
  float length_mm;  /**< Baseline length in millimeters. */
};

/**
 * ImuDataUnitProperty
 * @see senscord::kImuDataUnitPropertyKey
 */
#define SENSCORD_IMU_DATA_UNIT_PROPERTY_KEY     "imu_data_unit_property"

/**
 * @brief Property for obtaining unit of RawData.
 * @see senscord::ImuDataUnitProperty
 */
struct senscord_imu_data_unit_property_t {
  /** Unit of data of acceleration. */
  enum senscord_acceleration_unit_t acceleration;
  /** Unit of data of angular velocity. */
  enum senscord_angular_velocity_unit_t angular_velocity;
  /** Unit of data of magnetic field. */
  enum senscord_magnetic_field_unit_t magnetic_field;
  /** Unit of data of orientation. */
  enum senscord_orientation_unit_t orientation;
};

/**
 * SamplingFrequencyProperty.
 * @see senscord::kSamplingFrequencyPropertyKey
 */
#define SENSCORD_SAMPLING_FREQUENCY_PROPERTY_KEY \
    "sampling_frequency_property"

/**
 * @brief Set the sampling frequency in units of [Hz].
 */
typedef struct senscord_scalar_f_t senscord_sampling_frequency_property_t;

/**
 * AccelerometerRangeProperty.
 * @see senscord::kAccelerometerRangePropertyKey
 */
#define SENSCORD_ACCELEROMETER_RANGE_PROPERTY_KEY \
    "accelerometer_range_property"

/**
 * @brief Set the acceleration range.
 * @see senscord::AccelerometerRangeProperty
 */
typedef struct senscord_scalar_f_t senscord_accelerometer_range_property_t;

/**
 * GyrometerRangeProperty.
 * @see senscord::kGyrometerRangePropertyKey
 */
#define SENSCORD_GYROMETER_RANGE_PROPERTY_KEY  "gyrometer_range_property"

/**
 * @brief Set the gyrometer range.
 * @see senscord::GyrometerRangeProperty
 */
typedef struct senscord_scalar_f_t senscord_gyrometer_range_property_t;

/**
 * MagnetometerRangeProperty.
 * @see senscord::kMagnetometerRangePropertyKey
 */
#define SENSCORD_MAGNETOMETER_RANGE_PROPERTY_KEY \
    "magnetometer_range_property"

/**
 * @brief Set the magnetometer range.
 * @see senscord::MagnetometerRangeProperty
 */
typedef struct senscord_scalar_f_t senscord_magnetometer_range_property_t;

/**
 * MagnetometerRange3Property.
 * @see senscord::kMagnetometerRange3PropertyKey
 */
#define SENSCORD_MAGNETOMETER_RANGE3_PROPERTY_KEY \
    "magnetometer_range3_property"

/**
 * @brief Set the range of magnetometer for each xyz.
 * @see senscord::MagnetometerRange3Property
 */
typedef struct senscord_vector3f_t senscord_magnetometer_range3_property_t;

/**
 * AccelerationCalibProperty.
 * @see senscord::kAccelerationCalibPropertyKey
 */
#define SENSCORD_ACCELERATION_CALIB_PROPERTY_KEY \
    "acceleration_calib_property"

/**
 * @brief Property used for calibration of acceleration data.
 * @see senscord::AccelerationCalibProperty
 */
typedef struct senscord_axis_misalignment_t
    senscord_acceleration_calib_property_t;

/**
 * AngularVelocityCalibProperty.
 * @see senscord::kAngularVelocityCalibPropertyKey
 */
#define SENSCORD_ANGULAR_VELOCITY_CALIB_PROPERTY_KEY \
    "angular_velocity_calib_property"

/**
 * @brief Property used for calibration of angular velocity data.
 * @see senscord::AngularVelocityCalibProperty
 */
typedef struct senscord_axis_misalignment_t
    senscord_angular_velocity_calib_property_t;

/**
 * MagneticFieldCalibProperty.
 * @see senscord::kMagneticFieldCalibPropertyKey
 */
#define SENSCORD_MAGNETIC_FIELD_CALIB_PROPERTY_KEY \
    "magnetic_field_calib_property"

/**
 * @brief Property used for calibration of magnetic field data.
 * @see senscord::MagneticFieldCalibProperty
 */
typedef struct senscord_axis_misalignment_t
    senscord_magnetic_field_calib_property_t;

/**
 * MagneticNorthCalibProperty.
 * @see senscord::kMagneticNorthCalibPropertyKey
 */
#define SENSCORD_MAGNETIC_NORTH_CALIB_PROPERTY_KEY \
    "magnetic_north_calib_property"

/**
 * @brief Property for calibration magnetic north.
 * @see senscord::MagneticNorthCalibProperty
 */
struct senscord_magnetic_north_calib_property_t {
  float declination;    /**< Magnetic declination. */
  float inclination;    /**< Magnetic inclination. */
};

/**
 * SlamDataSupportedProperty
 * @see senscord::kSlamDataSupportedPropertyKey
 */
#define SENSCORD_SLAM_DATA_SUPPORTED_PROPERTY_KEY \
    "slam_data_supported_property"

/**
 * @brief Data format supported by SLAM stream.
 * @see senscord::SlamDataSupportedProperty
 */
struct senscord_slam_data_supported_property_t {
  uint8_t odometry_supported;   /**< Support for position and attitude data. */
  uint8_t gridmap_supported;     /**< GridMap support. */
  uint8_t pointcloud_supported;  /**< PointCloud support. */
};

/**
 * InitialPoseProperty
 *
 * For Property handled by this Key,
 * use senscord_pose_quaternion_data_t structure.
 * @see senscord::kInitialPosePropertyKey
 */
#define SENSCORD_INITIAL_POSE_PROPERTY_KEY \
    "initial_pose_property"

/**
 * PoseDataProperty
 * @see senscord::kPoseDataPropertyKey
 */
#define SENSCORD_POSE_DATA_PROPERTY_KEY     "pose_data_property"

/**
 * @brief Pose data format.
 */
/* Pose data : Quaternion */
#define SENSCORD_POSE_DATA_QUATERNION   "pose_data_quaternion"
/* Pose data : Matrix */
#define SENSCORD_POSE_DATA_MATRIX       "pose_data_matrix"

/**
 * @brief Pose data property.
 * @see senscord::PoseDataProperty
 */
struct senscord_pose_data_property_t {
  /** Format of pose data. */
  char data_format[SENSCORD_PIXEL_FORMAT_LENGTH];
};

/**
 * SlamOdometryProperty
 * @see senscord::kOdometryDataPropertyKey
 */
#define SENSCORD_ODOMETRY_DATA_PROPERTY_KEY     "odometry_data_property"

/**
 * @brief Position Data Property.
 * @see senscord::OdometryDataProperty
 */
struct senscord_odometry_data_property_t {
  /** Coordinate system. */
  enum senscord_coordinate_system_t coordinate_system;
};

/**
 * GridSizeProperty
 * @see senscord::kGridSizePropertyKey
 */
#define SENSCORD_GRID_SIZE_PROPERTY_KEY     "grid_size_property"

/**
 * @brief Grid size.
 * @see senscord::GridSize
 */
struct senscord_grid_size_t {
  float x;
  float y;
  float z;
  enum senscord_grid_unit_t unit;
};

/**
 * @brief Grid size Property.
 * @see senscord::GridSizeProperty
 */
typedef struct senscord_grid_size_t senscord_grid_size_property_t;

/**
 * GridMapProperty
 * @see senscord::kGridMapPropertyKey
 */
#define SENSCORD_GRID_MAP_PROPERTY_KEY          "grid_map_property"

/**
 * @brief Grid Data Property.
 * @see senscord::GridMapProperty
 */
struct senscord_grid_map_property_t {
  uint32_t grid_num_x;        /**< Number of x axis grids in grid map data. */
  uint32_t grid_num_y;        /**< Number of y axis grids in grid map data. */
  uint32_t grid_num_z;        /**< Number of z axis grids in grid map data. */
  /** Pixel format of the grid map. */
  char pixel_format[SENSCORD_PIXEL_FORMAT_LENGTH];
  struct senscord_grid_size_t grid_size;  /**< Size of grids. */
};

/**
 * PointCloudProperty
 * @see senscord::kPointCloudPropertyKey
 */
#define SENSCORD_POINT_CLOUD_PROPERTY_KEY       "point_cloud_property"

/**
 * @brief Point cloud Property.
 * @see senscord::PointCloudProperty
 */
struct senscord_point_cloud_property_t {
  uint32_t width;             /**< Width of the point cloud. */
  uint32_t height;            /**< Height of the point cloud. */

  /** The format of a pixel. */
  char pixel_format[SENSCORD_PIXEL_FORMAT_LENGTH];
};

/**
 * Property key of standard register 64 bit access.
 * @see senscord::kRegisterAccess64PropertyKey
 */
#define SENSCORD_REGISTER_ACCESS_64_PROPERTY_KEY "register_access_64_property"

/**
 * @brief Property of standard register 64 bit read/write access.
 * @see senscord::RegisterAccess64Property
 */
struct senscord_register_access_64_property_t {
  uint32_t id;       /**< Register ID. */
  uint64_t address;  /**< Target address. */
  uint64_t data;     /**< Writing data or read data. */
};

/**
 * Property key of standard register 32 bit access.
 * @see senscord::kRegisterAccess32PropertyKey
 */
#define SENSCORD_REGISTER_ACCESS_32_PROPERTY_KEY "register_access_32_property"

/**
 * @brief Property of standard register 32 bit read/write access.
 * @see senscord::RegisterAccess32Property
 */
struct senscord_register_access_32_property_t {
  uint32_t id;       /**< Register ID. */
  uint64_t address;  /**< Target address. */
  uint32_t data;     /**< Writing data or read data. */
};

/**
 * Property key of standard register 16 bit access.
 * @see senscord::kRegisterAccess16PropertyKey
 */
#define SENSCORD_REGISTER_ACCESS_16_PROPERTY_KEY "register_access_16_property"

/**
 * @brief Property of standard register 16 bit read/write access.
 * @see senscord::RegisterAccess16Property
 */
struct senscord_register_access_16_property_t {
  uint32_t id;       /**< Register ID. */
  uint64_t address;  /**< Target address. */
  uint16_t data;     /**< Writing data or read data. */
};

/**
 * Property key of standard register 8 bit access.
 * @see senscord::kRegisterAccess8PropertyKey
 */
#define SENSCORD_REGISTER_ACCESS_8_PROPERTY_KEY "register_access_8_property"

/**
 * @brief Property of standard register 8 bit read/write access.
 * @see senscord::RegisterAccess8Property
 */
struct senscord_register_access_8_property_t {
  uint32_t id;       /**< Register ID. */
  uint64_t address;  /**< Target address. */
  uint8_t data;      /**< Writing data or read data. */
};

/**
 * @brief Temperature information.
 * @see senscord::TemperatureInfo
 */
struct senscord_temperature_info_t {
  /** Sensor ID. */
  uint32_t sensor_id;
  /** Temperature data. */
  float temperature;
  /** Description of sensor. */
  char description[SENSCORD_TEMPERATURE_DESCRIPTION_LENGTH];
};

/**
 * TemperatureProperty
 * @see senscord::kTemperaturePropertyKey
 */
#define SENSCORD_TEMPERATURE_PROPERTY_KEY "temperature_property"

/**
 * @brief Property for the temperature.
 * @see senscord::TemperatureProperty
 */
struct senscord_temperature_property_t {
  /** Count of the array. */
  uint32_t count;
  /** Array of information for each temperature sensor. */
  struct senscord_temperature_info_t
    temperatures[SENSCORD_TEMPERATURE_LIST_MAX];
};

/**
 * DopCorrectionProperty
 * @see senscord::polarization::kDopCorrectionPropertyKey
 */
#define SENSCORD_POLARIZATION_DOP_CORRECTION_PROPERTY_KEY \
    "polarization_dop_correction_property"

/**
 * @brief Property for calclation of degree of polarization.
 * @see senscord::polarization::DopCorrectionProperty
 */
struct senscord_polarization_dop_correction_property_t {
  uint8_t noise_model;  /**< Enble correction. */
  float analog_gain;    /**< Gain for calclation. */
  float dop_gain;       /**< Gain for display. */
};

/**
 * InvalidMaskProperty
 * @see senscord::polarization::kInvalidMaskPropertyKey
 */
#define SENSCORD_POLARIZATION_INVALID_MASK_PROPERTY_KEY \
    "polarization_invalid_mask_property"

/**
 * @brief Property for specify invalid pixel of dop and normal image to display.
 * @see senscord::polarization::InvalidMaskProperty
 */
struct senscord_polarization_invalid_mask_property_t {
  uint8_t enable;               /**< Enble invalid pixel mask setting. */
  uint16_t pixel_white_threshold;  /**< Threshold of halation in pixels. */
  uint16_t pixel_black_threshold;  /**< Threshold of black defects in pixels. */
};

/**
 * @brief Color types for Normal Vector.
 * @see senscord::polarization::ColorType
 */
enum senscord_color_type_t {
  SENSCORD_COLOR_TYPE_RBG,
  SENSCORD_COLOR_TYPE_HSV
};

/**
 * NormalVectorProperty
 * @see senscord::polarization::kNormalVectorPropertyKey
 */
#define SENSCORD_POLARIZATION_NORMAL_VECTOR_PROPERTY_KEY \
    "polarization_normal_vector_property"

/**
 * @brief Property for specify the mode of expression for normal vector(RGB/HSV)
 * @see senscord::polarization::NormalVectorProperty
 */
struct senscord_polarization_normal_vector_property_t {
  /**< Mode of expression for normal vector(RGB/HSV). */
  enum senscord_color_type_t color_type;
  /**< Hue offset for HSV expression. */
  uint16_t rotation;
};

/**
 * ReflectionProperty
 * @see senscord::polarization::kReflectionPropertyKey
 */
#define SENSCORD_POLARIZATION_REFLECTION_PROPERTY_KEY \
    "polarization_reflection_property"

/**
 * @brief Property for specify reflection setting of polarized image.
 * @see senscord::polarization::ReflectionProperty
 */
struct senscord_polarization_reflection_property_t {
  float extraction_gain;  /**< Gain for display extraction image. */
};

/**
 * @brief Trigger types for TemporalContrastStream.
 * @see senscord::TemporalContrastTriggerType
 */
enum senscord_temporal_contrast_trigger_type_t {
  SENSCORD_TEMPORAL_CONTRAST_TRIGGER_TYPE_TIME,
  SENSCORD_TEMPORAL_CONTRAST_TRIGGER_TYPE_EVENT
};

/**
 * TemporalContrastDataProperty
 * @see senscord::kTemporalContrastDataPropertyKey
 */
#define SENSCORD_TEMPORAL_CONTRAST_DATA_PROPERTY \
    "pixel_polarity_data_property"

/**
 * @brief Property used for specify TemporalContrastStream's frame generation.
 * @see senscord::TemporalContrastDataProperty
 */
struct senscord_temporal_contrast_data_property_t {
  /** Frame generation trigger type. */
  enum senscord_temporal_contrast_trigger_type_t trigger_type;
  /** event count used when trigger type is SENSCORD_TRIGGER_TYPE_EVENT. */
  uint32_t event_count;
  /** the time used for image frame generation [usec]. */
  uint32_t accumulation_time;
};

/**
 * @brief Trigger types for PixelPolarityStream.
 * @see senscord::PixelPolarityTriggerType
 * @deprecated will be replaced by senscord_temporal_contrast_trigger_type_t
 */
enum senscord_pixel_polarity_trigger_type_t {
  SENSCORD_TRIGGER_TYPE_TIME,
  SENSCORD_TRIGGER_TYPE_EVENT
};

/**
 * PixelPolarityDataProperty
 * @see senscord::kPixelPolarityDataPropertyKey
 * @deprecated will be replaced by SENSCORD_TEMPORAL_CONTRAST_DATA_PROPERTY
 */
#define SENSCORD_PIXEL_POLARITY_DATA_PROPERTY \
    "pixel_polarity_data_property"

/**
 * @brief Property used for specify PixelPolarityStream's frame generation.
 * @see senscord::PixelPolarityDataProperty
 * @deprecated will be replaced by senscord_temporal_contrast_data_property_t
 */
struct senscord_pixel_polarity_data_property_t {
  /** Frame generation trigger type. */
  enum senscord_pixel_polarity_trigger_type_t trigger_type;
  /** event count used when trigger type is SENSCORD_TRIGGER_TYPE_EVENT. */
  uint32_t event_count;
  /** the time used for image frame generation [usec]. */
  uint32_t accumulation_time;
};

/**
 * RoiProperty
 * @see senscord::kRoiPropertyKey
 */
#define SENSCORD_ROI_PROPERTY_KEY        "roi_property"

/**
 * @brief Property for ROI setting.
 * @see senscord::RoiProperty
 */
struct senscord_roi_property_t {
  /** Horizontal offset of the top left corner of the cropping rectangle. */
  uint32_t left;
  /** Vertical offset of the top left corner of the cropping rectangle. */
  uint32_t top;
  uint32_t width;   /**< Width of the cropping rectangle. */
  uint32_t height;  /**< Height of the cropping rectangle. */
};

/**
 * ScoreThresholdProperty
 * @see senscord::kScoreThresholdPropertyKey
 */
#define SENSCORD_SCORE_THRESHOLD_PROPERTY_KEY \
    "score_threshold_property"

/**
 * @brief Property used for specify RecognitionStream frame generation.
 * @see senscord::ScoreThresholdProperty
 */
struct senscord_score_threshold_property_t {
  /** Frame generation trigger type. */
  float score_threshold;     /**< threshold for the score. */
};

/**
 * VelocityDataUnitProperty
 * @see senscord::kVelocityDataUnitPropertyKey
 */
#define SENSCORD_VELOCITY_DATA_UNIT_PROPERTY_KEY \
    "velocity_data_unit_property"

/**
 * @brief Property for obtaining unit of RawData.
 * @see senscord::VelocityDataUnitProperty
 */
struct senscord_velocity_data_unit_property_t {
  /** Unit of data of velocity. */
  enum senscord_velocity_unit_t velocity;
};

/**
 * DataRatePropertyKey
 */
#define SENSCORD_DATA_RATE_PROPERTY_KEY \
  "data_rate_property"

struct senscord_data_rate_element_t {
  float size;
  char name[SENSCORD_DATE_RATE_NAME_LENGTH];
  char unit[SENSCORD_DATE_RATE_UNIT_LENGTH];
};

struct senscord_data_rate_property_t {
  uint32_t count;
  struct senscord_data_rate_element_t
    elements[SENSCORD_DATA_RATE_ELEMENT_LIST_MAX];
};

/**
 * @see senscord::kCoordinateSystemPropertyKey
 */
#define SENSCORD_COORDINATE_SYSTEM_PROPERTY_KEY \
  "coordinate_system_property"

/**
 * @brief Property showing the information of coordinate system.
 * @see senscord::CoordinateSystemProperty
 */
struct senscord_coordinate_system_property_t {
  enum senscord_system_handed_t handed;  /**< System handed. */
  enum senscord_up_axis_t up_axis;  /**< Up axis information. */
  enum senscord_forward_axis_t forward_axis;  /**< Forward axis information. */
};

#endif  /* SENSCORD_C_API_PROPERTY_C_TYPES_H_ */
