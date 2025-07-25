/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SENSCORD_PROPERTY_TYPES_H_
#define SENSCORD_PROPERTY_TYPES_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include "senscord/config.h"
#include "senscord/serialize_define.h"
#include "senscord/property_types_audio.h"
#include "senscord/develop/property_utils.h"

namespace senscord {

/**
 * @brief Stream state definitions.
 */
enum StreamState {
  kStreamStateUndefined = 0,    /**< Undefined state */
  kStreamStateReady,            /**< Opened but not started */
  kStreamStateRunning,          /**< Started */
};

/**
 * @brief Frame buffering.
 */
enum Buffering {
  kBufferingUseConfig = -2,   /**< Buffering use config. */
  kBufferingDefault = -1,     /**< Buffering default. */
  kBufferingOff = 0,          /**< Buffering disable. */
  kBufferingOn,               /**< Buffering enable. */
};

/**
 * @brief Frame buffering format.
 */
enum BufferingFormat {
  kBufferingFormatUseConfig = -2,   /**< Use config. */
  kBufferingFormatDefault = -1, /**< Default format. */
  kBufferingFormatDiscard,      /**< Discard the latest frame. */
  kBufferingFormatOverwrite,    /**< Overwrite the oldest frame. */
  /** @deprecated "queue" has been replaced by "discard". */
  kBufferingFormatQueue = kBufferingFormatDiscard,
  /** @deprecated "ring" has been replaced by "overwrite". */
  kBufferingFormatRing = kBufferingFormatOverwrite
};

/**
 * @brief Frame replay speed with player.
 */
enum PlaySpeed {
  kPlaySpeedBasedOnFramerate = 0,   /**< Sending based on framerate. */
  /** @deprecated "BestEffort" replaces "FrameRate" in player component */
  kPlaySpeedBestEffort              /**< Sending without framerate. */
};

/**
 * @brief Encoding types for YUV (YCbCr).
 */
enum YCbCrEncoding {
  kEncodingUndefined,
  kEncodingBT601,
  kEncodingBT709,
  kEncodingBT2020,
  kEncodingBT2100
};

/**
 * @brief Quantization types for YUV (YCbCr).
 */
enum YCbCrQuantization {
  kQuantizationUndefined,
  kQuantizationFullRange,     // Y: 0-255,  C: 0-255
  kQuantizationLimitedRange,  // Y: 16-235, C: 16-240
  kQuantizationSuperWhite
};

/**
 * @brief Units used for acceleration.
 */
enum AccelerationUnit {
  kAccelerationNotSupported,  /**< sensor not supported. */
  kGravitational,             /**< Unit:[G]. */
  kMetrePerSecondSquared      /**< Unit:[m/s2]. */
};

/**
 * @brief Units used for angular velocity.
 */
enum AngularVelocityUnit {
  kAngularVelocityNotSupported,  /**< sensor not supported. */
  kDegreePerSecond,              /**< Unit:[deg/s]. */
  kRadianPerSecond               /**< Unit:[rad/s]. */
};

/**
 * @brief Units used for magnetic field.
 */
enum MagneticFieldUnit {
  kMagneticFieldNotSupported,  /**< sensor not supported. */
  kGauss,                      /**< Unit:[gauss]. */
  kMicroTesla                  /**< Unit:[uT]. */
};

/**
 * @brief Units used for orientataion.
 */
enum OrientationUnit {
  kOrientationNotSupported,  /**< sensor not supported. */
  kDegree,                   /**< Unit:[deg]. */
  kRadian                    /**< Unit:[rad]. */
};

/**
 * @brief Types of coordinate system.
 */
enum CoordinateSystem {
  kWorldCoordinate,  /**< World coordinate system. */
  kLocalCoordinate,  /**< Local coordinate system. */
  kCameraCoordinate  /**< Camera coordinate system. */
};

/**
 * @brief Units of grid.
 */
enum GridUnit {
  kGridPixel,       /**< Unit:[pixel]. */
  kGridMeter        /**< Unit:[m]. */
};

/**
 * @brief The field types of interlace.
 */
enum InterlaceField {
  kInterlaceFieldTop,       /**< Top field */
  kInterlaceFieldBottom     /**< Bottom field */
};

/**
 * @brief The order of interlace.
 */
enum InterlaceOrder {
  kInterlaceOrderTopFirst,        /**< Top first */
  kInterlaceOrderBottomFirst      /**< Bottom first */
};

/**
 * @brief The trigger types for TemporalContrast Stream
 */
enum TemporalContrastTriggerType {
  kTriggerTypeTime,        /**< Time based */
  kTriggerTypeEvent        /**< Event-number based */
};

/**
 * @brief The trigger types for PixelPolarity Stream
 * @deprecated will be replaced by TemporalContrastTriggerType
 */
typedef TemporalContrastTriggerType PixelPolarityTriggerType;

/**
 * @brief Color type.
 */
enum ColorType{
  kNormalVectorColorRGB = 0,   // rgb expression.
  kNormalVectorColorHSV,   // hsv expression.
};

/**
 * @brief Units used for velocity.
 */
enum VelocityUnit {
  kVelocityNotSupported,  /**< not supported. */
  kMetrePerSecond,        /**< Unit:[m/s]. */
  kPixelPerSecond         /**< Unit:[pixel/s]. */
};

/**
 * @brief System handed for CoordinateSystem.
 */
enum SystemHanded {
  kLeftHanded,       /**< Left-handed system. */
  kRightHanded,      /**< Right-handed system. */
};

/**
 * @brief Up axis for CoordinateSystem.
 */
enum UpAxis {
  kUpAxisUndefined,      /**< UpAxis undefined. */
  kUpAxisPlusX,          /**< X axis up. */
  kUpAxisPlusY,          /**< Y axis up. */
  kUpAxisPlusZ,          /**< Z axis up. */
  kUpAxisMinusX,         /**< X axis down. */
  kUpAxisMinusY,         /**< Y axis down. */
  kUpAxisMinusZ,         /**< Z axis down. */
};

/**
 * @brief Forward axis for CoordinateSystem.
 */
enum ForwardAxis {
  kForwardAxisUndefined,      /**< Forward undefined. */
  kForwardAxisPlusX,          /**< X axis up. */
  kForwardAxisPlusY,          /**< Y axis up. */
  kForwardAxisPlusZ,          /**< Z axis up. */
  kForwardAxisMinusX,         /**< X axis down. */
  kForwardAxisMinusY,         /**< Y axis down. */
  kForwardAxisMinusZ,         /**< Z axis down. */
};

}  // namespace senscord

SENSCORD_SERIALIZE_ADD_ENUM(senscord::StreamState)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::Buffering)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::BufferingFormat)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::PlaySpeed)

SENSCORD_SERIALIZE_ADD_ENUM(senscord::YCbCrEncoding)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::YCbCrQuantization)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::AccelerationUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::AngularVelocityUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::MagneticFieldUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::OrientationUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::CoordinateSystem)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::GridUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::InterlaceField)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::InterlaceOrder)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::TemporalContrastTriggerType)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::ColorType)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::VelocityUnit)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::SystemHanded)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::UpAxis)
SENSCORD_SERIALIZE_ADD_ENUM(senscord::ForwardAxis)

namespace senscord {

/**
 * Version property
 */
const char kVersionPropertyKey[] = "version_property";

/**
 * @brief Version information.
 */
struct VersionProperty {
  std::string name;         /**< Name. */
  uint32_t major;           /**< Major version. */
  uint32_t minor;           /**< Minor version. */
  uint32_t patch;           /**< Patch version. */
  std::string description;  /**< Version description. */

  SENSCORD_SERIALIZE_DEFINE(name, major, minor, patch, description)
};

/**
 * StreamTypeProperty
 */
const char kStreamTypePropertyKey[] = "stream_type_property";

/**
 * @brief Property for the type of the stream.
 */
struct StreamTypeProperty {
  std::string type;

  SENSCORD_SERIALIZE_DEFINE(type)
};

/**
 * StreamKeyProperty
 */
const char kStreamKeyPropertyKey[] = "stream_key_property";

/**
 * @brief Property for the key of the stream.
 */
struct StreamKeyProperty {
  std::string stream_key;

  SENSCORD_SERIALIZE_DEFINE(stream_key)
};

/**
 * StreamStateProperty
 */
const char kStreamStatePropertyKey[] = "stream_state_property";

/**
 * @brief Property for the current state of the stream.
 */
struct StreamStateProperty {
  StreamState state;

  SENSCORD_SERIALIZE_DEFINE(state)
};

/**
 * FrameBufferingProperty
 */
const char kFrameBufferingPropertyKey[] = "frame_buffering_property";

// Frame buffering number use xml
const int32_t kBufferNumUseConfig = -2;

// Frame buffering number default
const int32_t kBufferNumDefault = -1;

// Frame buffering number of unlimited
const int32_t kBufferNumUnlimited = 0;

/**
 * @brief Frame buffering setting.
 */
struct FrameBufferingProperty {
  Buffering buffering;      /**< Buffering enabling. */
  int32_t num;              /**< Max buffering frame number, */
  BufferingFormat format;   /**< Buffering format. */

  SENSCORD_SERIALIZE_DEFINE(buffering, num, format)
};

/**
 * CurrentFrameNumProperty
 */
const char kCurrentFrameNumPropertyKey[] = "current_frame_num_property";

/**
 * @brief Property for the current buffering frames.
 */
struct CurrentFrameNumProperty {
  int32_t arrived_number;     /**< Arrived number. */
  int32_t received_number;    /**< Received number. */

  SENSCORD_SERIALIZE_DEFINE(arrived_number, received_number)
};

/**
 * @brief Channel information.
 */
struct ChannelInfo {
  std::string raw_data_type;  /**< Type of raw data. */
  std::string description;    /**< Channel description. */

  SENSCORD_SERIALIZE_DEFINE(raw_data_type, description)
};

/**
 * ChannelInfoProperty
 */
const char kChannelInfoPropertyKey[] = "channel_info_property";

/**
 * @brief Property for channel information.
 */
struct ChannelInfoProperty {
  /** Channel information list. (Key=Channel ID) */
  std::map<uint32_t, ChannelInfo> channels;

  SENSCORD_SERIALIZE_DEFINE(channels)
};

/**
 * ChannelMaskProperty
 */
const char kChannelMaskPropertyKey[] = "channel_mask_property";

/**
 * @brief Property for masking the channel.
 */
struct ChannelMaskProperty {
  std::vector<uint32_t> channels;   /**< The list of masked channel IDs. */

  SENSCORD_SERIALIZE_DEFINE(channels)
};

/*
 * UserDataProperty
 */
const char kUserDataPropertyKey[] = "user_data_property";

/**
 * @brief Stream user data property.
 */
struct UserDataProperty {
  std::vector<uint8_t> data;  /**< User data bytes. */

  SENSCORD_SERIALIZE_DEFINE(data)
};

/**
 * RecordProperty
 */
const char kRecordPropertyKey[] = "record_property";

/**
 * @brief Property for the recording frames.
 */
struct RecordProperty {
  /**
   * State of recording.
   * If set to true, recording will start.
   * Startable only in the stream running state.
   */
  bool enabled;

  /**
   * Top directory path of recording files.
   * When to stop, this member is ignored.
   */
  std::string path;

  /**
   * The count of record frames.
   */
  uint32_t count;

  /**
   * Format names of each channel ID.
   * Frames of no specified channel ID will not be recorded.
   * For get the available formats, use RecorderListProperty.
   * When to stop, this member is ignored.
   */
  std::map<uint32_t, std::string> formats;

  /**
   * Number of the buffering of recording frame queue.
   * If set zero means the number equals one.
   * When to stop, this member is ignored.
   */
  uint32_t buffer_num;

  /**
   * Directory Naming Rules.
   * key is the directory type, value is a format string.
   * When to stop, this member is ignored.
   */
  std::map<std::string, std::string> name_rules;

  SENSCORD_SERIALIZE_DEFINE(
      enabled, path, count, formats, buffer_num, name_rules)
};

// Standard recording formats.
const char kRecordingFormatRaw[] = "raw";
const char kRecordingFormatCompositeRaw[] = "composite_raw";
const char kRecordingFormatSkv[] = "skv";

// Standard record directory type
const char kRecordDirectoryTop[] = "top";

/**
 * RecorderListProperty
 */
const char kRecorderListPropertyKey[] = "recorder_list_property";

/**
 * @brief Property for reference the available recording formats.
 *       For stream property getting only.
 */
struct RecorderListProperty {
  std::vector<std::string> formats;   /**< list of formats */

  SENSCORD_SERIALIZE_DEFINE(formats)
};

/**
 * PlayModeProperty
 */
const char kPlayModePropertyKey[] = "play_mode_property";

/**
 * @brief Property for the mode of replay the stream.
 */
struct PlayModeProperty {
  bool repeat;          /**< Enabling the repeat play. */

  SENSCORD_SERIALIZE_DEFINE(repeat)
};

/**
 * PlayProperty
 */
const char kPlayPropertyKey[] = "play_property";

/**
 * @brief Property for the settings before replay stream.
 */
struct PlayProperty {
  std::string target_path;        /**< Path of the recorded data. */
  uint32_t start_offset;          /**< Offset of starting frame. */
  uint32_t count;                 /**< Playing frames from start_offset. */
  PlaySpeed speed;                /**< Play speed. */
  PlayModeProperty mode;          /**< Play mode. */

  SENSCORD_SERIALIZE_DEFINE(target_path, start_offset, count,
      speed, mode)
};

// play count for playing all frames from start_offset.
const uint64_t kPlayCountAll = 0;

/**
 * PlayFileInfoProperty
 */
const char kPlayFileInfoPropertyKey[] = "play_file_info_property";

/**
 * @brief Property for playback file information.
 */
struct PlayFileInfoProperty {
  std::string target_path;        /**< Directory path of recorded. */
  std::string record_date;        /**< Recorded date and time. */
  std::string stream_key;         /**< Stream key to recorded. */
  std::string stream_type;        /**< Stream type to recorded. */
  uint32_t frame_count;           /**< Number of Frames recorded. */

  SENSCORD_SERIALIZE_DEFINE(
      target_path, record_date, stream_key, stream_type, frame_count)
};

/**
 * PlayPositionProperty
 */
const char kPlayPositionPropertyKey[] = "play_position_property";

/**
 * @brief Property that indicates the playback position in the player function.
 */
struct PlayPositionProperty {
  uint32_t position;

  SENSCORD_SERIALIZE_DEFINE(position)
};

/**
 * @brief Property for set/get binary data.
 */
struct BinaryProperty {
  std::vector<uint8_t> data;

  SENSCORD_SERIALIZE_DEFINE(data)
};

/**
 * @brief Vector2.
 */
template <typename T>
struct Vector2 {
  T x;
  T y;

  SENSCORD_SERIALIZE_DEFINE(x, y)
};

/**
 * @brief Vector3.
 */
template <typename T>
struct Vector3 {
  T x;
  T y;
  T z;

  SENSCORD_SERIALIZE_DEFINE(x, y, z)
};

/**
 * @brief Vector4.
 */
template <typename T>
struct Vector4 {
  T x;
  T y;
  T z;
  T a;

  SENSCORD_SERIALIZE_DEFINE(x, y, z, a)
};

/**
 * @brief Quaternion.
 */
template <typename T>
struct Quaternion {
  T x;
  T y;
  T z;
  T w;

  SENSCORD_SERIALIZE_DEFINE(x, y, z, w)
};

/**
 * @brief Matrix(3x3).
 * @deprecated will be replaced by Matrix3x3
 */
template <typename T>
struct Matrix {
  T element[3][3];

  SENSCORD_SERIALIZE_DEFINE(element)
};
template <typename T>
struct Matrix3x3 {
  T element[3][3];

  SENSCORD_SERIALIZE_DEFINE(element)
};

/**
 * @brief Matrix(3x4).
 */
template <typename T>
struct Matrix3x4 {
  T element[3][4];

  SENSCORD_SERIALIZE_DEFINE(element)
};

/**
 * @brief Scalar.
 */
template <typename T>
struct Scalar {
  T value;

  SENSCORD_SERIALIZE_DEFINE(value)
};

/**
 * @brief Range expressed by the min max.
 */
template <typename T>
struct Range {
  T min;
  T max;

  SENSCORD_SERIALIZE_DEFINE(min, max)
};

/**
 * @brief Misalignment of the axis direction.
 */
struct AxisMisalignment {
  Matrix3x3<float> ms;
  Vector3<float> offset;

  SENSCORD_SERIALIZE_DEFINE(ms, offset)
};

/**
 * @brief Structure for the region of plane for AE or ROI, etc.
 */

struct RectangleRegionParameter {
  // region of rectangle.
  uint32_t top;     /* upper position of region from origin  */
  uint32_t left;    /* left  position of region from origin  */
  uint32_t bottom;  /* bottom position of region from origin */
  uint32_t right;   /* right position of region from origin  */

  SENSCORD_SERIALIZE_DEFINE(top, left, bottom, right)
};

/**
 * PresetListProperty
 */
const char kPresetListPropertyKey[] = "preset_list_property";

/**
 * @brief Property for the list of property's preset IDs.
 */
struct PresetListProperty {
  std::map<uint32_t, std::string> presets;    /**< Preset ID + description */

  SENSCORD_SERIALIZE_DEFINE(presets)
};

/**
 * PresetProperty
 */
const char kPresetPropertyKey[] = "preset_property";

/**
 * @brief Property for the property's preset.
 */
struct PresetProperty {
  uint32_t id;            /**< Preset ID */

  SENSCORD_SERIALIZE_DEFINE(id)
};

/**
 * ImageProperty
 */
const char kImagePropertyKey[] = "image_property";

/**
 * @brief Structures that handle properties of Raw data of Image and Depth data.
 */
struct ImageProperty {
  uint32_t width;         /**< Image width. */
  uint32_t height;        /**< Image height. */
  uint32_t stride_bytes;  /**< Image stride. */
  std::string pixel_format;   /**< The format of a pixel. */

  SENSCORD_SERIALIZE_DEFINE(width, height, stride_bytes, pixel_format)
};

/**
 * ConfidenceProperty
 */
const char kConfidencePropertyKey[] = "confidence_property";

/**
 * @brief Structures that handle properties of raw data of confidence.
 */
struct ConfidenceProperty {
  uint32_t width;         /**< Image width. */
  uint32_t height;        /**< Image height. */
  uint32_t stride_bytes;  /**< Image stride. */
  std::string pixel_format;   /**< The format of a pixel. */

  SENSCORD_SERIALIZE_DEFINE(width, height, stride_bytes, pixel_format)
};

/**
 * @brief Pixel formats
 */
// Packed RGB
const char kPixelFormatARGB444[] = "image_argb444";   // ARGB 4444
const char kPixelFormatXRGB444[] = "image_xrgb444";   // XRGB 4444
const char kPixelFormatRGB24[] = "image_rgb24";       // RGB 888
const char kPixelFormatARGB32[] = "image_argb32";     // ARGB 8888
const char kPixelFormatXRGB32[] = "image_xrgb32";     // XRGB 8888
const char kPixelFormatBGR24[] = "image_bgr24";       // BGR 888
const char kPixelFormatABGR32[] = "image_abgr32";     // ABGR 8888
const char kPixelFormatXBGR32[] = "image_xbgr32";     // XBGR 8888

// Planar RGB
const char kPixelFormatRGB8Planar[] = "image_rgb8_planar";    // RGB 8-bit
const char kPixelFormatRGB16Planar[] = "image_rgb16_planar";  // RGB 16-bit

// Greyscale
const char kPixelFormatGREY[] = "image_grey";   // 8-bit Greyscale
const char kPixelFormatY10[] = "image_y10";     // 10-bit Greyscale (on 16bit)
const char kPixelFormatY12[] = "image_y12";     // 12-bit Greyscale (on 16bit)
const char kPixelFormatY14[] = "image_y14";     // 14-bit Greyscale (on 16bit)
const char kPixelFormatY16[] = "image_y16";     // 16-bit Greyscale
const char kPixelFormatY20[] = "image_y20";     // 20-bit Greyscale (on 32bit)
const char kPixelFormatY24[] = "image_y24";     // 24-bit Greyscale (on 32bit)

// YUV
const char kPixelFormatYUV444[] = "image_yuv444";     // YUV444
const char kPixelFormatNV12[] = "image_nv12";         // YUV420SP
const char kPixelFormatNV16[] = "image_nv16";         // YUV422SP
const char kPixelFormatYUV420[] = "image_yuv420";     // YUV420
const char kPixelFormatYUV422P[] = "image_yuv422p";   // YUV422P
const char kPixelFormatYUYV[] = "image_yuyv";         // YUYV
const char kPixelFormatUYVY[] = "image_uyvy";         // UYVY

// Bayer
const char kPixelFormatSBGGR8[] = "image_sbggr8";
const char kPixelFormatSGBRG8[] = "image_sgbrg8";
const char kPixelFormatSGRBG8[] = "image_sgrbg8";
const char kPixelFormatSRGGB8[] = "image_srggb8";

const char kPixelFormatSBGGR10[] = "image_sbggr10";
const char kPixelFormatSGBRG10[] = "image_sgbrg10";
const char kPixelFormatSGRBG10[] = "image_sgrbg10";
const char kPixelFormatSRGGB10[] = "image_srggb10";

const char kPixelFormatSBGGR12[] = "image_sbggr12";
const char kPixelFormatSGBRG12[] = "image_sgbrg12";
const char kPixelFormatSGRBG12[] = "image_sgrbg12";
const char kPixelFormatSRGGB12[] = "image_srggb12";

// Quad Bayer
const char kPixelFormatQuadSBGGR8[] = "image_quad_sbggr8";
const char kPixelFormatQuadSGBRG8[] = "image_quad_sgbrg8";
const char kPixelFormatQuadSGRBG8[] = "image_quad_sgrbg8";
const char kPixelFormatQuadSRGGB8[] = "image_quad_srggb8";

const char kPixelFormatQuadSBGGR10[] = "image_quad_sbggr10";
const char kPixelFormatQuadSGBRG10[] = "image_quad_sgbrg10";
const char kPixelFormatQuadSGRBG10[] = "image_quad_sgrbg10";
const char kPixelFormatQuadSRGGB10[] = "image_quad_srggb10";

const char kPixelFormatQuadSBGGR12[] = "image_quad_sbggr12";
const char kPixelFormatQuadSGBRG12[] = "image_quad_sgbrg12";
const char kPixelFormatQuadSGRBG12[] = "image_quad_sgrbg12";
const char kPixelFormatQuadSRGGB12[] = "image_quad_srggb12";

// Polarization image
const char kPixelFormatPolar_90_45_135_0_Y8[] =
    "image_polar_90_45_135_0_y8";
const char kPixelFormatPolar_90_45_135_0_Y10[] =
    "image_polar_90_45_135_0_y10";
const char kPixelFormatPolar_90_45_135_0_Y12[] =
    "image_polar_90_45_135_0_y12";
const char kPixelFormatPolar_90_45_135_0_RGGB8[] =
    "image_polar_90_45_135_0_rggb8";
const char kPixelFormatPolar_90_45_135_0_RGGB10[] =
    "image_polar_90_45_135_0_rggb10";
const char kPixelFormatPolar_90_45_135_0_RGGB12[] =
    "image_polar_90_45_135_0_rggb12";

// Compressed image
const char kPixelFormatJPEG[] = "image_jpeg";
const char kPixelFormatH264[] = "image_h264";

// Depth
const char kPixelFormatZ16[] = "depth_z16";     // 16-bit Z-Depth
const char kPixelFormatZ32F[] = "depth_z32f";   // 32-bit float Z-Depth
const char kPixelFormatD16[] = "depth_d16";     // 16-bit Disparity

// Confidence
const char kPixelFormatC1P[] = "confidence_c1p";    // 1-bit positive confidence
const char kPixelFormatC1N[] = "confidence_c1n";    // 1-bit negative confidence
const char kPixelFormatC16[] = "confidence_c16";    // 16-bit confidence
const char kPixelFormatC32F[] = "confidence_c32f";  // 32-bit float confidence

// PointCloud
// signed   16-bit (x, y, depth)
const char kPixelFormatXYZ16[]       = "point_cloud_xyz16";
// signed   16-bit (x, y, depth, rgb)
const char kPixelFormatXYZRGB16[]    = "point_cloud_xyzrgb16";
// signed   32-bit (x, y, depth)
const char kPixelFormatXYZ32[]       = "point_cloud_xyz32";
// signed   32-bit (x, y, depth, rgb)
const char kPixelFormatXYZRGB32[]    = "point_cloud_xyzrgb32";
// unsigned 16-bit (x, y, depth)
const char kPixelFormatXYZ16U[]       = "point_cloud_xyz16u";
// unsigned 16-bit (x, y, depth, rgb)
const char kPixelFormatXYZRGB16U[]    = "point_cloud_xyzrgb16u";
// unsigned 32-bit (x, y, depth)
const char kPixelFormatXYZ32U[]       = "point_cloud_xyz32u";
// unsigned 32-bit (x, y, depth, rgb)
const char kPixelFormatXYZRGB32U[]    = "point_cloud_xyzrgb32u";
// signed   32-bit float (x, y, depth)
const char kPixelFormatXYZ32F[]       = "point_cloud_xyz32f";
// signed   32-bit float (x, y, depth, rgb)
const char kPixelFormatXYZRGB32F[]    = "point_cloud_xyzrgb32f";
// signed   16-bit signed (x, y, depth) planar array
const char kPixelFormatXYZ16Planar[]  = "point_cloud_xyz16_planar";
// unsigned 16-bit unsigned (x, y, depth) planar array
const char kPixelFormatXYZ16UPlanar[] = "point_cloud_xyz16u_planar";
// signed   32-bit float(x, y, depth) planar array
const char kPixelFormatXYZ32FPlanar[] = "point_cloud_xyz32f_planar";

// GridMap
// 1-bit positive voxel data (obstacle, unknown) planer array
// (obstacle) 1 : an obstacle / 0 : no obstacle
// (unknown)  1 : unknown whether there is an obstacle or not
//            0 : either an obstacle or no obstacle
const char kPixelFormatGridMap1P1N[] = "grid_map_1p1n";

/**
 * ColorSpaceProperty
 */
const char kColorSpacePropertyKey[] = "color_space_property";

/**
 * @brief Property of color space type for YUV.
 */
struct ColorSpaceProperty {
  YCbCrEncoding encoding;
  YCbCrQuantization quantization;

  SENSCORD_SERIALIZE_DEFINE(encoding, quantization)
};

/**
 * FrameRateProperty
 */
const char kFrameRatePropertyKey[] = "frame_rate_property";

/**
 * @brief Structure for setting frame rate.
 *        Specify in the style of numerator / denominator.
 *        ex) 60fps : num = 60, denom = 1
 */
struct FrameRateProperty {
  uint32_t num;    /**< Framerate numerator. */
  uint32_t denom;  /**< Framerate denominator. */

  SENSCORD_SERIALIZE_DEFINE(num, denom)
};

/**
 * SkipFrameProperty
 */
const char kSkipFramePropertyKey[] = "skip_frame_property";

/**
 * @brief Structure for setting the skip rate of the frame.
 *
 * If 'rate = 1' is specified, frames are not skipped.
 * If 'rate = N' (N is 2 or more) is specified, the frame is skipped and
 * the frame rate drops to 1 / N.
 */
struct SkipFrameProperty {
  uint32_t rate;

  SENSCORD_SERIALIZE_DEFINE(rate)
};

/**
 * LensProperty
 */
const char kLensPropertyKey[] = "lens_property";

/**
 * @brief Structure used to acquire field angle of camera.
 */
struct LensProperty {
  /** The horizontal viewing angle of the lens. */
  float horizontal_field_of_view;
  /** The vertical viewing angle of the lens. */
  float vertical_field_of_view;

  SENSCORD_SERIALIZE_DEFINE(horizontal_field_of_view, vertical_field_of_view)
};

/**
 * DepthProperty
 */
const char kDepthPropertyKey[] = "depth_property";

/**
 * @brief Structure for handling Depth data properties.
 */
struct DepthProperty {
  /**
   * Scale of the depth value, in metres.
   * By multiplying this value, the depth value is converted to metres.
   */
  float scale;
  float depth_min_range;   /**< Minimum depth value of the sensor. */
  float depth_max_range;   /**< Maximum depth value of the sensor. */

  SENSCORD_SERIALIZE_DEFINE(scale, depth_min_range, depth_max_range)
};

/**
 * ImageSensorFunctionProperty
 */
const char kImageSensorFunctionPropertyKey[] =
    "image_sensor_function_property";

/**
 * @brief Structures used to set the functions used in the sensor.
 */
struct ImageSensorFunctionProperty {
  bool auto_exposure;       /**< Setting auto exposure function. */
  bool auto_white_balance;  /**< Setting automatic white balance function. */
  int32_t brightness;       /**< Brightness value. */
  uint32_t iso_sensitivity; /**< ISO sensitivity. (100,200,400,800,1600,...) */
  uint32_t exposure_time;   /**< Time of exposure [100usec]. */
  std::string exposure_metering;  /**< Exposure metering mode. */
  float gamma_value;        /**< Gamma correction value */
  uint32_t gain_value;      /**< Gain value. */
  int32_t hue;              /**< Hue value. */
  int32_t saturation;       /**< Saturation value. */
  int32_t sharpness;        /**< Sharpness value. */
  int32_t white_balance;    /**< White balance value. */

  SENSCORD_SERIALIZE_DEFINE(auto_exposure, auto_white_balance, brightness,
      iso_sensitivity, exposure_time, exposure_metering, gamma_value,
      gain_value, hue, saturation, sharpness, white_balance)
};

/**
 * @brief Fixed values for the exposure.
 */
// Exposure time : Auto
const uint32_t kExposureTimeAuto = 0;
// ISO Sensitivity : Auto
const uint32_t kIsoSensitivityAuto = 0;

/**
 * @brief Exposure metering modes.
 */
// Metering mode : None (Disable)
const char kExposureMeteringNone[] = "none";
// Metering mode : Average
const char kExposureMeteringAverage[] = "average";
// Metering mode : Center weighted
const char kExposureMeteringCenterWeighted[] = "center_weighted";
// Metering mode : Spot
const char kExposureMeteringSpot[] = "spot";
// Metering mode : Matrix
const char kExposureMeteringMatrix[] = "matrix";

/**
 * ImageSensorFunctionSupportedProperty
 */
const char kImageSensorFunctionSupportedPropertyKey[] =
    "image_sensor_function_supported_property";

/**
 * @brief Structure for acquiring functions supported by Component.
 *
 * For each function of the image sensor, set the counter corresponding
 * to Componet as a boolean value.
 */
struct ImageSensorFunctionSupportedProperty {
  bool auto_exposure_supported;
  bool auto_white_balance_supported;
  bool brightness_supported;
  bool iso_sensitivity_supported;
  bool exposure_time_supported;
  bool exposure_metering_supported;
  bool gamma_value_supported;
  bool gain_value_supported;
  bool hue_supported;
  bool saturation_supported;
  bool sharpness_supported;
  bool white_balance_supported;

  SENSCORD_SERIALIZE_DEFINE(auto_exposure_supported,
      auto_white_balance_supported, brightness_supported,
      iso_sensitivity_supported,
      exposure_time_supported, exposure_metering_supported,
      gamma_value_supported, gain_value_supported, hue_supported,
      saturation_supported, sharpness_supported, white_balance_supported)
};

/**
 * ExposureProperty
 */
const char kExposurePropertyKey[] = "exposure_property";

/**
 * @brief Structure for the image of the camera exposure.
 */
struct ExposureProperty {
  std::string mode;           /**< Mode of exposure. */
  float ev_compensation;      /**< Compensation value of EV. */
  uint32_t exposure_time;     /**< Time of exposure. [usec] */
  uint32_t iso_sensitivity;   /**< ISO sensitivity. (100,200,400,800 ...) */
  std::string metering;       /**< Exposure metering mode. */

  /** target region of the camera exposure. */
  RectangleRegionParameter target_region;

  SENSCORD_SERIALIZE_DEFINE(mode, ev_compensation, exposure_time,
      iso_sensitivity, metering, target_region)
};

/**
 * @brief Exposure modes.
 */
// Exposure mode : Auto
const char kExposureModeAuto[] = "auto";
// Exposure mode : Hold
const char kExposureModeHold[] = "hold";
// Exposure mode : Manual
const char kExposureModeManual[] = "manual";
// Exposure mode : Gain Fix
const char kExposureModeGainFix[] = "gainfix";
// Exposure mode : Time Fix
const char kExposureModeTimeFix[] = "timefix";

/**
 * WhiteBalanceProperty
 */
const char kWhiteBalancePropertyKey[] = "white_balance_property";

/**
 * @brief Structure for the white balance.
 */
struct WhiteBalanceProperty {
  std::string mode;     /**< Mode of white balance. */

  SENSCORD_SERIALIZE_DEFINE(mode)
};

/**
 * @brief White balance modes.
 */
// White balance mode : Auto
const char kWhiteBalanceModeAuto[] = "auto";
// White balance mode : Manual
const char kWhiteBalanceModeManual[] = "manual";

/**
 * @brief Structure for handling internal parameters of calibration.
 */
struct IntrinsicCalibrationParameter {
  float cx;  /**< The x-axis coordinate of the optical center point. */
  float cy;  /**< The y-axis coordinate of the optical center point. */
  float fx;  /**< Focal length on x axis. */
  float fy;  /**< Focal length on y axis. */
  float s;   /**< skewness. */

  SENSCORD_SERIALIZE_DEFINE(cx, cy, fx, fy, s)
};

/**
 * @brief Structure for handling extrinsic parameters of calibration.
 */
struct ExtrinsicCalibrationParameter {
  float r11;           /**< Extrinsic parameter r11. */
  float r12;           /**< Extrinsic parameter r12. */
  float r13;           /**< Extrinsic parameter r13. */
  float r21;           /**< Extrinsic parameter r21. */
  float r22;           /**< Extrinsic parameter r22. */
  float r23;           /**< Extrinsic parameter r23. */
  float r31;           /**< Extrinsic parameter r31. */
  float r32;           /**< Extrinsic parameter r32. */
  float r33;           /**< Extrinsic parameter r33. */
  float t1;            /**< Extrinsic parameter t1. */
  float t2;            /**< Extrinsic parameter t2. */
  float t3;            /**< Extrinsic parameter t3. */
  Matrix3x4<float> p;  /**< Extrinsic parameter p11-p34. */

  SENSCORD_SERIALIZE_DEFINE(r11, r12, r13, r21, r22, r23, r31, r32, r33,
                            t1, t2, t3, p)
};

/**
 * @brief Structure for handling camera distortion coefficient.
 */
struct DistortionCalibrationParameter {
  float k1;  /**< Camera distortion coefficient k1. */
  float k2;  /**< Camera distortion coefficient k2. */
  float k3;  /**< Camera distortion coefficient k3. */
  float k4;  /**< Camera distortion coefficient k4. */
  float k5;  /**< Camera distortion coefficient k5. */
  float k6;  /**< Camera distortion coefficient k6. */
  float p1;  /**< Camera distortion coefficient p1. */
  float p2;  /**< Camera distortion coefficient p1. */

  SENSCORD_SERIALIZE_DEFINE(k1, k2, k3, k4, k5, k6, p1, p2)
};

/**
 * @brief Calibration parameters of a single camera.
 */
struct CameraCalibrationParameters {
  /** Camera internal parameters. */
  IntrinsicCalibrationParameter intrinsic;
  /** Distortion correction coefficient. */
  DistortionCalibrationParameter distortion;
  /** Extrinsic parameters. */
  ExtrinsicCalibrationParameter extrinsic;

  SENSCORD_SERIALIZE_DEFINE(intrinsic, distortion, extrinsic)
};

/**
 * CameraCalibrationProperty
 */
const char kCameraCalibrationPropertyKey[] =
    "camera_calibration_property";

/**
 * @brief Property for camera calibration.
 */
struct CameraCalibrationProperty {
  /** List of camera calibration parameters. */
  std::map<uint32_t, CameraCalibrationParameters> parameters;

  SENSCORD_SERIALIZE_DEFINE(parameters)
};

/**
 * InterlaceProperty
 */
const char kInterlacePropertyKey[] = "interlace_property";

/**
 * @brief Channel's property for interlace.
 */
struct InterlaceProperty {
  InterlaceField field;       /**< contained field type. */

  SENSCORD_SERIALIZE_DEFINE(field)
};

/**
 * InterlaceInfoProperty
 */
const char kInterlaceInfoPropertyKey[] = "interlace_info_property";

/**
 * @brief Property for interlace informations.
 */
struct InterlaceInfoProperty {
  InterlaceOrder order;       /**< order of field */

  SENSCORD_SERIALIZE_DEFINE(order)
};

/**
 * ImageCropProperty
 */
const char kImageCropPropertyKey[] = "image_crop_property";

/**
 * @brief Property for image cropping.
 */
struct ImageCropProperty {
  /** Horizontal offset of the top left corner of the cropping rectangle. */
  uint32_t left;
  /** Vertical offset of the top left corner of the cropping rectangle. */
  uint32_t top;
  uint32_t width;     /**< Width of the cropping rectangle. */
  uint32_t height;    /**< Height of the cropping rectangle. */

  SENSCORD_SERIALIZE_DEFINE(left, top, width, height)
};

/**
 * ImageCropBoundsProperty
 */
const char kImageCropBoundsPropertyKey[] = "image_crop_bounds_property";

/**
 * @brief This Property is there to represent the bounds of the crop.
 */
struct ImageCropBoundsProperty {
  /** Horizontal offset of the cropable rectangle area. In pixels. */
  uint32_t left;
  /** Vertical offset of the cropable rectangle. In pixels. */
  uint32_t top;
  /** Width of the cropable rectangle from the offset. In pixels. */
  uint32_t width;
  /** Height of the cropable rectangle from the offset. In pixels. */
  uint32_t height;

  SENSCORD_SERIALIZE_DEFINE(left, top, width, height)
};

/**
 * BaselineLengthProperty
 */
const char kBaselineLengthPropertyKey[] = "baseline_length_property";

/**
 * @brief Structure for handling baseline length between cameras.
 */
struct BaselineLengthProperty {
  float length_mm;  /**< Baseline length in millimeters. */

  SENSCORD_SERIALIZE_DEFINE(length_mm)
};

/**
 * ImuDataUnitProperty
 */
const char kImuDataUnitPropertyKey[] = "imu_data_unit_property";

/**
 * @brief Property for obtaining unit of RawData.
 */
struct ImuDataUnitProperty {
  /** Unit of data of acceleration. */
  AccelerationUnit acceleration;
  /** Unit of data of angular velocity. */
  AngularVelocityUnit angular_velocity;
  /** Unit of data of the magnetic field. */
  MagneticFieldUnit magnetic_field;
  /** Unit of data of the orientation. */
  OrientationUnit orientation;

  SENSCORD_SERIALIZE_DEFINE(acceleration, angular_velocity, magnetic_field,
                            orientation)
};

/**
 * SamplingFrequencyProperty
 */
const char kSamplingFrequencyPropertyKey[] = "sampling_frequency_property";

/**
 * @brief Set the sampling frequency in units of [Hz].
 */
typedef Scalar<float> SamplingFrequencyProperty;


/**
 * AccelerometerRangeProperty.
 */
const char kAccelerometerRangePropertyKey[] = "accelerometer_range_property";

/**
 * @brief Set the acceleration range.
 */
typedef Scalar<float> AccelerometerRangeProperty;

/**
 * GyrometerRangeProperty.
 */
const char kGyrometerRangePropertyKey[] = "gyrometer_range_property";

/**
 * @brief Set the gyrometer range.
 */
typedef Scalar<float> GyrometerRangeProperty;

/**
 * MagnetometerRangeProperty.
 */
const char kMagnetometerRangePropertyKey[] = "magnetometer_range_property";

/**
 * @brief Set the magnetometer range.
 */
typedef Scalar<float> MagnetometerRangeProperty;

/**
 * MagnetometerRange3Property.
 */
const char kMagnetometerRange3PropertyKey[] = "magnetometer_range3_property";

/**
 * @brief Set the range of magnetometer for each xyz.
 */
typedef Vector3<float> MagnetometerRange3Property;

/**
 * AccelerationCalibProperty.
 */
const char kAccelerationCalibPropertyKey[] = "acceleration_calib_property";

/**
 * @brief Property used for calibration of acceleration data.
 */
typedef AxisMisalignment AccelerationCalibProperty;

/**
 * AngularVelocityCalibProperty.
 */
const char kAngularVelocityCalibPropertyKey[]
    = "angular_velocity_calib_property";

/**
 * @brief Property used for calibration of angular velocity data.
 */
typedef AxisMisalignment AngularVelocityCalibProperty;

/**
 * MagneticFieldCalibProperty.
 */
const char kMagneticFieldCalibPropertyKey[] = "magnetic_field_calib_property";

/**
 * @brief Property used for calibration of magnetic field data.
 */
typedef AxisMisalignment MagneticFieldCalibProperty;

/**
 * MagneticNorthCalibProperty.
 */
const char kMagneticNorthCalibPropertyKey[] = "magnetic_north_calib_property";

/**
 * @brief Property for calibration magnetic north.
 */
struct MagneticNorthCalibProperty {
  float declination;
  float inclination;

  SENSCORD_SERIALIZE_DEFINE(declination, inclination)
};

/**
 * SlamDataSupportedProperty
 */
const char kSlamDataSupportedPropertyKey[] = "slam_data_supported_property";

/**
 * @brief Data format supported by SLAM stream.
 */
struct SlamDataSupportedProperty {
  bool odometry_supported;   /**< Support for position and attitude data. */
  bool gridmap_supported;     /**< GridMap support. */
  bool pointcloud_supported;  /**< PointCloud support. */

  SENSCORD_SERIALIZE_DEFINE(odometry_supported, gridmap_supported,
                            pointcloud_supported)
};

/**
 * InitialPoseProperty
 *
 * For Property handled by this Key, use PoseData structure.
 */
const char kInitialPosePropertyKey[] = "initial_pose_property";

/**
 * PoseDataProperty
 */
const char kPoseDataPropertyKey[] = "pose_data_property";

// Pose data format
const char kPoseDataFormatQuaternion[] = "pose_data_quaternion";
const char kPoseDataFormatMatrix[] = "pose_data_matrix";

/**
 * @brief Pose data property.
 */
struct PoseDataProperty {
  std::string data_format;  /**< format of pose data. */
  SENSCORD_SERIALIZE_DEFINE(data_format)
};

/**
 * OdometryDataProperty
 */
const char kOdometryDataPropertyKey[] = "odometry_data_property";

/**
 * @brief Odometry Data Property.
 */
struct OdometryDataProperty {
  CoordinateSystem coordinate_system;  /**< Coordinate system. */

  SENSCORD_SERIALIZE_DEFINE(coordinate_system)
};


/**
 * GridSizeProperty
 */
const char kGridSizePropertyKey[] = "grid_size_property";

/**
 * @brief Grid size Property.
 */
struct GridSize {
  float x;                    /**< grid size of x axis */
  float y;                    /**< grid size of y axis */
  float z;                    /**< grid size of z axis */
  GridUnit unit;              /**< unit of x grid */
  SENSCORD_SERIALIZE_DEFINE(x, y, z, unit)
};
typedef GridSize GridSizeProperty;

/**
 * GridMapProperty
 */
const char kGridMapPropertyKey[] = "grid_map_property";

/**
 * @brief Grid map Property.
 */
struct GridMapProperty {
  uint32_t grid_num_x;        /**< number of x axis grids in grid map data */
  uint32_t grid_num_y;        /**< number of y axis grids in grid map data */
  uint32_t grid_num_z;        /**< number of z axis grids in grid map data */
  std::string pixel_format;   /**< pixel format of the grid map. */
  GridSize grid_size;         /**< size of grids. */

  SENSCORD_SERIALIZE_DEFINE(grid_num_x, grid_num_y, grid_num_z, pixel_format,
                            grid_size)
};

/**
 * PointCloudProperty
 */
const char kPointCloudPropertyKey[] = "point_cloud_property";

/**
 * @brief Point cloud Property.
 *    If the cloud is unordered, height is 1 and width is the length of
 *    the point cloud.
 */
struct PointCloudProperty {
  uint32_t width;             /**< Width of the point cloud. */
  uint32_t height;            /**< Height of the point cloud. */
  std::string pixel_format;   /**< The format of a pixel. */

  SENSCORD_SERIALIZE_DEFINE(width, height, pixel_format)
};

// Property key of standard register 64 bit access.
const char kRegisterAccess64PropertyKey[] = "register_access_64_property";

// Property key of standard register 32 bit access.
const char kRegisterAccess32PropertyKey[] = "register_access_32_property";

// Property key of standard register 16 bit access.
const char kRegisterAccess16PropertyKey[] = "register_access_16_property";

// Property key of standard register 8 bit access.
const char kRegisterAccess8PropertyKey[] = "register_access_8_property";

/**
 * @brief Information for single register access.
 */
template <typename T>
struct RegisterAccessElement {
  uint64_t address;  /**< Target address. */
  T data;            /**< Writing data or read data. */

  SENSCORD_SERIALIZE_DEFINE(address, data)
};

/**
 * @brief Property of standard register read/write access.
 */
template <typename T>
struct RegisterAccessProperty {
  /** Register ID. */
  uint32_t id;
  /** RegisterAccessElement array. */
  std::vector<RegisterAccessElement<T> > element;

  SENSCORD_SERIALIZE_DEFINE(id, element)
};

/**
 * @brief Property of standard register 64 bit read/write access.
 */
typedef RegisterAccessProperty<uint64_t> RegisterAccess64Property;

/**
 * @brief Property of standard register 32 bit read/write access.
 */
typedef RegisterAccessProperty<uint32_t> RegisterAccess32Property;

/**
 * @brief Property of standard register 16 bit read/write access.
 */
typedef RegisterAccessProperty<uint16_t> RegisterAccess16Property;

/**
 * @brief Property of standard register 8 bit read/write access.
 */
typedef RegisterAccessProperty<uint8_t> RegisterAccess8Property;

/**
 * @brief Property for the temperature information.
 */
struct TemperatureInfo {
  float temperature;         /**< Temperature data. */
  std::string description;   /**< Description of sensor. */

  SENSCORD_SERIALIZE_DEFINE(temperature, description)
};

/**
 * TemperatureProperty
 */
const char kTemperaturePropertyKey[] = "temperature_property";

/**
 * @brief Property for the temperature.
 */
struct TemperatureProperty {
  /** Information for each temperature sensor. (Key = Sensor id) */
  std::map<uint32_t, TemperatureInfo> temperatures;

  SENSCORD_SERIALIZE_DEFINE(temperatures)
};

/**
 * TemporalContrastDataProperty
 */
const char kTemporalContrastDataPropertyKey[] = "pixel_polarity_data_property";

/**
 * @brief A property for TemporalContrast stream.
 */
struct TemporalContrastDataProperty {
  /* Specify the trigger type to use */
  TemporalContrastTriggerType trigger_type;
  /* The number of events, used with event-number-based trigger only */
  uint32_t event_count;
  /* The exposure-like time span, used with time-based trigger only [usec] */
  uint32_t accumulation_time;

  SENSCORD_SERIALIZE_DEFINE(trigger_type, event_count,
                            accumulation_time)
};

/**
 * PixelPolarityDataProperty
 * @deprecated will be replaced by TemporalContrastDataPropertyKey
 */
const char kPixelPolarityDataPropertyKey[] = "pixel_polarity_data_property";

/**
 * @brief A property for PixelPolarity stream.
 * @deprecated will be replaced by TemporalContrastDataProperty
 */
typedef TemporalContrastDataProperty PixelPolarityDataProperty;

/**
 * ROIProperty
 */
const char kRoiPropertyKey[] = "roi_property";

/**
 * @brief ROI setting for devices
 */
struct RoiProperty {
  /** Horizontal offset of the top left corner of the cropping rectangle. */
  uint32_t left;
  /** Vertical offset of the top left corner of the cropping rectangle. */
  uint32_t top;
  uint32_t width;     /**< Width of the cropping rectangle. */
  uint32_t height;    /**< Height of the cropping rectangle. */

  SENSCORD_SERIALIZE_DEFINE(left, top, width, height)
};

/**
 * PolarizationDopCorrectionProperty
 */
const char kPolarizationDopCorrectionPropertyKey[] =
  "polarization_dop_correction_property";

/**
 * @brief parameter for calclation of degree of polarization.
 */
struct PolarizationDopCorrectionProperty {
  bool noise_model;   /**< enble correction. */
  float analog_gain;  /**< gain for calclation. */
  float dop_gain;     /**< gain for display. */
  SENSCORD_SERIALIZE_DEFINE(noise_model, analog_gain, dop_gain)
};

/**
 * PolarizationInvalidMaskProperty
 */
const char kPolarizationInvalidMaskPropertyKey[] =
  "polarization_invalid_mask_property";

/**
 * @brief parameter to specify invalid pixel of dop and normal image to display.
 */
struct PolarizationInvalidMaskProperty {
  bool enable;                     /**< enble invalid pixel mask setting. */
  uint16_t pixel_white_threshold;  /**< threshold of halation in pixels. */
  uint16_t pixel_black_threshold;  /**< threshold of black defects in pixels. */
  SENSCORD_SERIALIZE_DEFINE(
    enable, pixel_white_threshold, pixel_black_threshold)
};

/**
 * PolarizationNormalVectorProperty
 */
const char kPolarizationNormalVectorPropertyKey[] =
  "polarization_normal_vector_property";

/**
 * @brief parameter to specify the mode of expression for normal vector(RGB/HSV)
 */
struct PolarizationNormalVectorProperty {
  /** mode of expression for normal vector(RGB/HSV). */
  ColorType color_type;
  /** hue offset for HSV expression. */
  uint16_t rotation;
  SENSCORD_SERIALIZE_DEFINE(color_type, rotation)
};


/**
 * PolarizationReflectionProperty
 */
const char kPolarizationReflectionPropertyKey[] =
  "polarization_reflection_property";

/**
 * @brief parameter to specify reflection setting of polarized image.
 */
struct PolarizationReflectionProperty {
  float extraction_gain;     /**< gain for display extraction image. */
  SENSCORD_SERIALIZE_DEFINE(extraction_gain)
};

/**
 * ScoreThresholdProperty
 */
const char kScoreThresholdPropertyKey[] =
  "score_threshold_property";

/**
 * @brief parameter to specify the threshold for the score to be output.
 */
struct ScoreThresholdProperty {
  float score_threshold;     /**< threshold for the score. */
  SENSCORD_SERIALIZE_DEFINE(score_threshold)
};

/**
 * VelocityDataUnitProperty
 */
const char kVelocityDataUnitPropertyKey[] = "velocity_data_unit_property";

/**
 * @brief Property for obtaining unit of RawData.
 */
struct VelocityDataUnitProperty {
  /** Unit of data of velocity. */
  VelocityUnit velocity;
  SENSCORD_SERIALIZE_DEFINE(velocity)
};

/**
 * @brief Structure for data rate.
 */
struct DataRateElement {
  float size; /**< data rate size. */
  std::string name; /**< data rate name. */
  std::string unit; /**< data rate unit. */
  SENSCORD_SERIALIZE_DEFINE(size, name, unit)
};

/**
 * DataRateProperty
 */
const char kDataRatePropertyKey[] = "data_rate_property";

/**
 * @brief Property data rate elements.
 */
struct DataRateProperty {
  std::vector<DataRateElement> elements; /**< array of element. */
  SENSCORD_SERIALIZE_DEFINE(elements)
};

/**
 * CoordinateSystemProperty
 */
const char kCoordinateSystemPropertyKey[] = "coordinate_system_property";

/**
 * @brief Property showing the information of coordinate system.
 */
struct CoordinateSystemProperty  {
  SystemHanded handed; /**< System handed. */
  UpAxis up_axis; /**< Up axis information. */
  ForwardAxis forward_axis; /**< Forward axis information. */
  SENSCORD_SERIALIZE_DEFINE(handed, up_axis, forward_axis)
};

}   // namespace senscord

#endif  // SENSCORD_PROPERTY_TYPES_H_
