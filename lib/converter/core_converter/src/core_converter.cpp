/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core_converter.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

// export register function.
SENSCORD_REGISTER_CONVERTER(senscord::CoreConverterLibrary)

namespace senscord
{

  /**
   * @brief Initialize the converter library.
   * @param[in] collector  Converter collector.
   * @return Status object.
   */
  Status CoreConverterLibrary::Init(ConverterCollector *collector)
  {
    // Property
    collector->Add<senscord_version_property_t, VersionProperty>(
        kVersionPropertyKey, this);
    collector->Add<senscord_stream_type_property_t, StreamTypeProperty>(
        kStreamTypePropertyKey, this);
    collector->Add<senscord_stream_key_property_t, StreamKeyProperty>(
        kStreamKeyPropertyKey, this);
    collector->Add<senscord_stream_state_property_t, StreamStateProperty>(
        kStreamStatePropertyKey, this);
    collector->Add<senscord_frame_buffering_property_t, FrameBufferingProperty>(
        kFrameBufferingPropertyKey, this);
    collector->Add<senscord_current_frame_num_property_t, CurrentFrameNumProperty>(
        kCurrentFrameNumPropertyKey, this);
    collector->Add<senscord_channel_info_property_t, ChannelInfoProperty>(
        kChannelInfoPropertyKey, this);
    collector->Add<senscord_channel_mask_property_t, ChannelMaskProperty>(
        kChannelMaskPropertyKey, this);
#ifdef SENSCORD_RECORDER
    collector->Add<senscord_record_property_t, RecordProperty>(
        kRecordPropertyKey, this);
    collector->Add<senscord_recorder_list_property_t, RecorderListProperty>(
        kRecorderListPropertyKey, this);
#endif // SENSCORD_RECORDER
#ifdef SENSCORD_PLAYER
    collector->Add<senscord_play_file_info_property_t, PlayFileInfoProperty>(
        kPlayFileInfoPropertyKey, this);
    collector->Add<senscord_play_mode_property_t, PlayModeProperty>(
        kPlayModePropertyKey, this);
    collector->Add<senscord_play_property_t, PlayProperty>(
        kPlayPropertyKey, this);
    collector->Add<senscord_play_position_property_t, PlayPositionProperty>(
        kPlayPositionPropertyKey, this);
#endif // SENSCORD_PLAYER
    collector->Add<senscord_preset_list_property_t, PresetListProperty>(
        kPresetListPropertyKey, this);
    collector->Add<senscord_preset_property_t, PresetProperty>(
        kPresetPropertyKey, this);
    collector->Add<senscord_image_property_t, ImageProperty>(
        kImagePropertyKey, this);
    collector->Add<senscord_image_crop_property_t, ImageCropProperty>(
        kImageCropPropertyKey, this);
    collector->Add<senscord_image_crop_bounds_property_t, ImageCropBoundsProperty>(
        kImageCropBoundsPropertyKey, this);
    collector->Add<senscord_confidence_property_t, ConfidenceProperty>(
        kConfidencePropertyKey, this);
    collector->Add<senscord_color_space_property_t, ColorSpaceProperty>(
        kColorSpacePropertyKey, this);
    collector->Add<senscord_frame_rate_property_t, FrameRateProperty>(
        kFrameRatePropertyKey, this);
    collector->Add<senscord_skip_frame_property_t, SkipFrameProperty>(
        kSkipFramePropertyKey, this);
    collector->Add<senscord_lens_property_t, LensProperty>(
        kLensPropertyKey, this);
    collector->Add<senscord_depth_property_t, DepthProperty>(
        kDepthPropertyKey, this);
    collector->Add<senscord_image_sensor_function_property_t, ImageSensorFunctionProperty>(
        kImageSensorFunctionPropertyKey, this);
    collector->Add<senscord_image_sensor_function_supported_property_t, ImageSensorFunctionSupportedProperty>(
        kImageSensorFunctionSupportedPropertyKey, this);
    collector->Add<senscord_exposure_property_t, ExposureProperty>(
        kExposurePropertyKey, this);
    collector->Add<senscord_white_balance_property_t, WhiteBalanceProperty>(
        kWhiteBalancePropertyKey, this);
    collector->Add<senscord_camera_calibration_property_t, CameraCalibrationProperty>(
        kCameraCalibrationPropertyKey, this);
    collector->Add<senscord_interlace_property_t, InterlaceProperty>(
        kInterlacePropertyKey, this);
    collector->Add<senscord_interlace_info_property_t, InterlaceInfoProperty>(
        kInterlaceInfoPropertyKey, this);
    collector->Add<senscord_base_line_length_property_t, BaselineLengthProperty>(
        kBaselineLengthPropertyKey, this);
    collector->Add<senscord_imu_data_unit_property_t, ImuDataUnitProperty>(
        kImuDataUnitPropertyKey, this);
    collector->Add<senscord_scalar_f_t, Scalar<float>>(
        kSamplingFrequencyPropertyKey, this);
    collector->Add<senscord_scalar_f_t, Scalar<float>>(
        kAccelerometerRangePropertyKey, this);
    collector->Add<senscord_scalar_f_t, Scalar<float>>(
        kGyrometerRangePropertyKey, this);
    collector->Add<senscord_scalar_f_t, Scalar<float>>(
        kMagnetometerRangePropertyKey, this);
    collector->Add<senscord_vector3f_t, Vector3<float>>(
        kMagnetometerRange3PropertyKey, this);
    collector->Add<senscord_axis_misalignment_t, AxisMisalignment>(
        kAccelerationCalibPropertyKey, this);
    collector->Add<senscord_axis_misalignment_t, AxisMisalignment>(
        kAngularVelocityCalibPropertyKey, this);
    collector->Add<senscord_axis_misalignment_t, AxisMisalignment>(
        kMagneticFieldCalibPropertyKey, this);
    collector->Add<senscord_magnetic_north_calib_property_t, MagneticNorthCalibProperty>(
        kMagneticNorthCalibPropertyKey, this);
    collector->Add<senscord_slam_data_supported_property_t, SlamDataSupportedProperty>(
        kSlamDataSupportedPropertyKey, this);
    collector->Add(
        kInitialPosePropertyKey, static_cast<PoseDataConverter *>(this));
    collector->Add<senscord_pose_data_property_t, PoseDataProperty>(
        kPoseDataPropertyKey, this);
    collector->Add<senscord_odometry_data_property_t, OdometryDataProperty>(
        kOdometryDataPropertyKey, this);
    collector->Add<senscord_grid_size_property_t, GridSizeProperty>(
        kGridSizePropertyKey, this);
    collector->Add<senscord_grid_map_property_t, GridMapProperty>(
        kGridMapPropertyKey, this);
    collector->Add<senscord_point_cloud_property_t, PointCloudProperty>(
        kPointCloudPropertyKey, this);
    collector->Add<senscord_register_access_64_property_t, RegisterAccess64Property>(
        kRegisterAccess64PropertyKey, this);
    collector->Add<senscord_register_access_32_property_t, RegisterAccess32Property>(
        kRegisterAccess32PropertyKey, this);
    collector->Add<senscord_register_access_16_property_t, RegisterAccess16Property>(
        kRegisterAccess16PropertyKey, this);
    collector->Add<senscord_register_access_8_property_t, RegisterAccess8Property>(
        kRegisterAccess8PropertyKey, this);
    collector->Add<senscord_temperature_property_t, TemperatureProperty>(
        kTemperaturePropertyKey, this);
    collector->Add<senscord_polarization_dop_correction_property_t, PolarizationDopCorrectionProperty>(
        kPolarizationDopCorrectionPropertyKey, this);
    collector->Add<senscord_polarization_invalid_mask_property_t, PolarizationInvalidMaskProperty>(
        kPolarizationInvalidMaskPropertyKey, this);
    collector->Add<senscord_polarization_normal_vector_property_t, PolarizationNormalVectorProperty>(
        kPolarizationNormalVectorPropertyKey, this);
    collector->Add<senscord_polarization_reflection_property_t, PolarizationReflectionProperty>(
        kPolarizationReflectionPropertyKey, this);
    collector->Add<senscord_temporal_contrast_data_property_t, TemporalContrastDataProperty>(
        kTemporalContrastDataPropertyKey, this);
    collector->Add<senscord_roi_property_t, RoiProperty>(
        kRoiPropertyKey, this);
    collector->Add<senscord_score_threshold_property_t, ScoreThresholdProperty>(
        kScoreThresholdPropertyKey, this);
    collector->Add<senscord_velocity_data_unit_property_t, VelocityDataUnitProperty>(
        kVelocityDataUnitPropertyKey, this);
    collector->Add<senscord_data_rate_property_t, DataRateProperty>(
        kDataRatePropertyKey, this);
    collector->Add<senscord_coordinate_system_property_t, CoordinateSystemProperty>(
        kCoordinateSystemPropertyKey, this);
    collector->Add<senscord_audio_property_t, AudioProperty>(
        kAudioPropertyKey, this);
    collector->Add<senscord_audio_pcm_property_t, AudioPcmProperty>(
        kAudioPcmPropertyKey, this);
    // RawData
    collector->Add<senscord_vector3f_t, Vector3<float>>(
        kRawDataTypeAcceleration, this);
    collector->Add<senscord_vector3f_t, Vector3<float>>(
        kRawDataTypeAngularVelocity, this);
    collector->Add<senscord_vector3f_t, Vector3<float>>(
        kRawDataTypeMagneticField, this);
    collector->Add<senscord_rotation_data_t, RotationData>(
        kRawDataTypeRotation, this);
    collector->Add(
        kRawDataTypePose, static_cast<PoseDataConverter *>(this));

    return Status::OK();
  }

  // Scalar<float>
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_scalar_f_t &src, Scalar<float> *dst)
  {
    dst->value = src.value;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const Scalar<float> &src, senscord_scalar_f_t *dst)
  {
    dst->value = src.value;
    return Status::OK();
  }

  // Vector3<float>
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_vector3f_t &src, Vector3<float> *dst)
  {
    dst->x = src.x;
    dst->y = src.y;
    dst->z = src.z;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const Vector3<float> &src, senscord_vector3f_t *dst)
  {
    dst->x = src.x;
    dst->y = src.y;
    dst->z = src.z;
    return Status::OK();
  }

  // AxisMisalignment
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_axis_misalignment_t &src, AxisMisalignment *dst)
  {
    for (int32_t i = 0; i < 3; ++i)
    {
      for (int32_t j = 0; j < 3; ++j)
      {
        dst->ms.element[i][j] = src.ms.element[i][j];
      }
    }
    dst->offset.x = src.offset.x;
    dst->offset.y = src.offset.y;
    dst->offset.z = src.offset.z;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const AxisMisalignment &src, senscord_axis_misalignment_t *dst)
  {
    for (int32_t i = 0; i < 3; ++i)
    {
      for (int32_t j = 0; j < 3; ++j)
      {
        dst->ms.element[i][j] = src.ms.element[i][j];
      }
    }
    dst->offset.x = src.offset.x;
    dst->offset.y = src.offset.y;
    dst->offset.z = src.offset.z;
    return Status::OK();
  }

  // VersionProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_version_property_t &src, VersionProperty *dst)
  {
    dst->name = src.name;
    dst->major = src.major;
    dst->minor = src.minor;
    dst->patch = src.patch;
    dst->description = src.description;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const VersionProperty &src, senscord_version_property_t *dst)
  {
    StringToCharArray(src.name, dst->name);
    dst->major = src.major;
    dst->minor = src.minor;
    dst->patch = src.patch;
    StringToCharArray(src.description, dst->description);
    return Status::OK();
  }

  // StreamTypeProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_stream_type_property_t &src, StreamTypeProperty *dst)
  {
    dst->type = src.type;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const StreamTypeProperty &src, senscord_stream_type_property_t *dst)
  {
    StringToCharArray(src.type, dst->type);
    return Status::OK();
  }

  // StreamKeyProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_stream_key_property_t &src, StreamKeyProperty *dst)
  {
    dst->stream_key = src.stream_key;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const StreamKeyProperty &src, senscord_stream_key_property_t *dst)
  {
    StringToCharArray(src.stream_key, dst->stream_key);
    return Status::OK();
  }

  // StreamStateProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_stream_state_property_t &src, StreamStateProperty *dst)
  {
    dst->state = static_cast<StreamState>(src.state);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const StreamStateProperty &src, senscord_stream_state_property_t *dst)
  {
    dst->state = static_cast<senscord_stream_state_t>(src.state);
    return Status::OK();
  }

  // FrameBufferingProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_frame_buffering_property_t &src,
      FrameBufferingProperty *dst)
  {
    dst->buffering = static_cast<Buffering>(src.buffering);
    dst->num = src.num;
    dst->format = static_cast<BufferingFormat>(src.format);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const FrameBufferingProperty &src,
      senscord_frame_buffering_property_t *dst)
  {
    dst->buffering = static_cast<senscord_buffering_t>(src.buffering);
    dst->num = src.num;
    dst->format = static_cast<senscord_buffering_format_t>(src.format);
    return Status::OK();
  }

  // CurrentFrameNumProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_current_frame_num_property_t &src,
      CurrentFrameNumProperty *dst)
  {
    dst->arrived_number = src.arrived_number;
    dst->received_number = src.received_number;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const CurrentFrameNumProperty &src,
      senscord_current_frame_num_property_t *dst)
  {
    dst->arrived_number = src.arrived_number;
    dst->received_number = src.received_number;
    return Status::OK();
  }

  // ChannelInfoProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_channel_info_property_t &src, ChannelInfoProperty *dst)
  {
    for (uint32_t i = 0; i < src.count; ++i)
    {
      ChannelInfo info = {};
      info.raw_data_type = src.channels[i].raw_data_type;
      info.description = src.channels[i].description;
      dst->channels.insert(std::make_pair(src.channels[i].channel_id, info));
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ChannelInfoProperty &src, senscord_channel_info_property_t *dst)
  {
    if (src.channels.size() > SENSCORD_CHANNEL_LIST_MAX)
    {
      dst->count = SENSCORD_CHANNEL_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.channels.size());
    }
    std::map<uint32_t, ChannelInfo>::const_iterator itr =
        src.channels.begin();
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      senscord_channel_info_t &dst_info = dst->channels[i];
      dst_info.channel_id = itr->first;
      StringToCharArray(itr->second.raw_data_type, dst_info.raw_data_type);
      StringToCharArray(itr->second.description, dst_info.description);
      ++itr;
    }
    return Status::OK();
  }

  // ChannelMaskProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_channel_mask_property_t &src, ChannelMaskProperty *dst)
  {
    dst->channels.reserve(src.count);
    for (uint32_t i = 0; i < src.count; ++i)
    {
      dst->channels.push_back(src.channels[i]);
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ChannelMaskProperty &src, senscord_channel_mask_property_t *dst)
  {
    if (src.channels.size() > SENSCORD_CHANNEL_LIST_MAX)
    {
      dst->count = SENSCORD_CHANNEL_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.channels.size());
    }
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      dst->channels[i] = src.channels[i];
    }
    return Status::OK();
  }

#ifdef SENSCORD_RECORDER
  // RecordProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_record_property_t &src, RecordProperty *dst)
  {
    dst->enabled = src.enabled;
    dst->path = src.path;
    dst->count = src.count;
    for (uint32_t i = 0; i < src.info_count; ++i)
    {
      dst->formats.insert(std::make_pair(
          src.info_array[i].channel_id, src.info_array[i].format.name));
    }
    dst->buffer_num = src.buffer_num;
    for (uint32_t i = 0; i < src.name_rules_count; ++i)
    {
      dst->name_rules.insert(std::make_pair(
          src.name_rules[i].directory_type, src.name_rules[i].format));
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RecordProperty &src, senscord_record_property_t *dst)
  {
    dst->enabled = src.enabled;
    if (src.path.size() + 1 > sizeof(dst->path))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "path is too long (in=%" PRIuS ", max=%" PRIuS ")",
          src.path.size() + 1, sizeof(dst->path));
    }
    StringToCharArray(src.path, dst->path);
    {
      dst->info_count = static_cast<uint32_t>(src.formats.size());
      std::map<uint32_t, std::string>::const_iterator itr = src.formats.begin();
      for (uint32_t i = 0; i < dst->info_count; ++i)
      {
        senscord_record_info_t &dst_info = dst->info_array[i];
        dst_info.channel_id = itr->first;
        StringToCharArray(itr->second, dst_info.format.name);
        ++itr;
      }
    }
    dst->buffer_num = src.buffer_num;
    {
      dst->name_rules_count = static_cast<uint32_t>(src.name_rules.size());
      std::map<std::string, std::string>::const_iterator itr =
          src.name_rules.begin();
      for (uint32_t i = 0; i < dst->name_rules_count; ++i)
      {
        senscord_record_name_rules_t &dst_info = dst->name_rules[i];
        StringToCharArray(itr->first, dst_info.directory_type);
        StringToCharArray(itr->second, dst_info.format);
        ++itr;
      }
    }
    return Status::OK();
  }

  // RecorderListProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_recorder_list_property_t &src, RecorderListProperty *dst)
  {
    dst->formats.reserve(src.count);
    for (uint32_t i = 0; i < src.count; ++i)
    {
      dst->formats.push_back(src.formats[i].name);
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RecorderListProperty &src, senscord_recorder_list_property_t *dst)
  {
    dst->count = static_cast<uint32_t>(src.formats.size());
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      StringToCharArray(src.formats[i], dst->formats[i].name);
    }
    return Status::OK();
  }
#endif // SENSCORD_RECORDER

#ifdef SENSCORD_PLAYER
  // PlayFileInfoProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_play_file_info_property_t &src, PlayFileInfoProperty *dst)
  {
    dst->target_path = src.target_path;
    dst->record_date = src.record_date;
    dst->stream_key = src.stream_key;
    dst->stream_type = src.stream_type;
    dst->frame_count = src.frame_count;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PlayFileInfoProperty &src, senscord_play_file_info_property_t *dst)
  {
    if (src.target_path.size() + 1 > sizeof(dst->target_path))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "target_path is too long (in=%" PRIuS ", max=%" PRIuS ")",
          src.target_path.size() + 1, sizeof(dst->target_path));
    }
    StringToCharArray(src.target_path, dst->target_path);
    // following will be trimmed down to match the others
    StringToCharArray(src.record_date, dst->record_date);
    StringToCharArray(src.stream_key, dst->stream_key);
    StringToCharArray(src.stream_type, dst->stream_type);
    dst->frame_count = src.frame_count;
    return Status::OK();
  }

  // PlayModeProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_play_mode_property_t &src, PlayModeProperty *dst)
  {
    dst->repeat = src.repeat;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PlayModeProperty &src, senscord_play_mode_property_t *dst)
  {
    dst->repeat = src.repeat;
    return Status::OK();
  }

  // PlayProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_play_property_t &src, PlayProperty *dst)
  {
    dst->target_path = src.target_path;
    dst->start_offset = src.start_offset;
    dst->count = src.count;
    dst->speed = static_cast<PlaySpeed>(src.speed);
    dst->mode.repeat = src.mode.repeat;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PlayProperty &src, senscord_play_property_t *dst)
  {
    if (src.target_path.size() + 1 > sizeof(dst->target_path))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "target_path is too long (in=%" PRIuS ", max=%" PRIuS ")",
          src.target_path.size() + 1, sizeof(dst->target_path));
    }
    StringToCharArray(src.target_path, dst->target_path);
    dst->start_offset = src.start_offset;
    dst->count = src.count;
    dst->speed = static_cast<senscord_play_speed_t>(src.speed);
    dst->mode.repeat = src.mode.repeat;
    return Status::OK();
  }

  // PlayPositionProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_play_position_property_t &src, PlayPositionProperty *dst)
  {
    dst->position = src.position;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PlayPositionProperty &src, senscord_play_position_property_t *dst)
  {
    dst->position = src.position;
    return Status::OK();
  }
#endif // SENSCORD_PLAYER

  // PresetListProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_preset_list_property_t &src, PresetListProperty *dst)
  {
    for (uint32_t i = 0; i < src.count; ++i)
    {
      dst->presets.insert(
          std::make_pair(src.presets[i].id, src.presets[i].description));
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PresetListProperty &src, senscord_preset_list_property_t *dst)
  {
    if (src.presets.size() > SENSCORD_PRESET_LIST_MAX)
    {
      dst->count = SENSCORD_PRESET_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.presets.size());
    }
    std::map<uint32_t, std::string>::const_iterator itr = src.presets.begin();
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      senscord_preset_info_t &dst_info = dst->presets[i];
      dst_info.id = itr->first;
      StringToCharArray(itr->second, dst_info.description);
      ++itr;
    }
    return Status::OK();
  }

  // PresetProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_preset_property_t &src, PresetProperty *dst)
  {
    dst->id = src.id;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PresetProperty &src, senscord_preset_property_t *dst)
  {
    dst->id = src.id;
    return Status::OK();
  }

  // ImageProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_image_property_t &src, ImageProperty *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    dst->stride_bytes = src.stride_bytes;
    dst->pixel_format = src.pixel_format;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImageProperty &src, senscord_image_property_t *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    dst->stride_bytes = src.stride_bytes;
    StringToCharArray(src.pixel_format, dst->pixel_format);
    return Status::OK();
  }

  // ImageCropProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_image_crop_property_t &src, ImageCropProperty *dst)
  {
    dst->left = src.left;
    dst->top = src.top;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImageCropProperty &src, senscord_image_crop_property_t *dst)
  {
    dst->left = src.left;
    dst->top = src.top;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  // ImageCropBoundsProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_image_crop_bounds_property_t &src,
      ImageCropBoundsProperty *dst)
  {
    dst->left = src.left;
    dst->top = src.top;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImageCropBoundsProperty &src,
      senscord_image_crop_bounds_property_t *dst)
  {
    dst->left = src.left;
    dst->top = src.top;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  // ConfidenceProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_confidence_property_t &src, ConfidenceProperty *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    dst->stride_bytes = src.stride_bytes;
    dst->pixel_format = src.pixel_format;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ConfidenceProperty &src, senscord_confidence_property_t *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    dst->stride_bytes = src.stride_bytes;
    StringToCharArray(src.pixel_format, dst->pixel_format);
    return Status::OK();
  }

  // ColorSpaceProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_color_space_property_t &src, ColorSpaceProperty *dst)
  {
    dst->encoding = static_cast<YCbCrEncoding>(src.encoding);
    dst->quantization = static_cast<YCbCrQuantization>(src.quantization);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ColorSpaceProperty &src, senscord_color_space_property_t *dst)
  {
    dst->encoding = static_cast<senscord_ycbcr_encoding_t>(src.encoding);
    dst->quantization =
        static_cast<senscord_ycbcr_quantization_t>(src.quantization);
    return Status::OK();
  }

  // FrameRateProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_frame_rate_property_t &src, FrameRateProperty *dst)
  {
    dst->num = src.num;
    dst->denom = src.denom;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const FrameRateProperty &src, senscord_frame_rate_property_t *dst)
  {
    dst->num = src.num;
    dst->denom = src.denom;
    return Status::OK();
  }

  // SkipFrameProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_skip_frame_property_t &src, SkipFrameProperty *dst)
  {
    dst->rate = src.rate;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const SkipFrameProperty &src, senscord_skip_frame_property_t *dst)
  {
    dst->rate = src.rate;
    return Status::OK();
  }

  // LensProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_lens_property_t &src, LensProperty *dst)
  {
    dst->horizontal_field_of_view = src.horizontal_field_of_view;
    dst->vertical_field_of_view = src.vertical_field_of_view;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const LensProperty &src, senscord_lens_property_t *dst)
  {
    dst->horizontal_field_of_view = src.horizontal_field_of_view;
    dst->vertical_field_of_view = src.vertical_field_of_view;
    return Status::OK();
  }

  // DepthProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_depth_property_t &src, DepthProperty *dst)
  {
    dst->scale = src.scale;
    dst->depth_min_range = src.depth_min_range;
    dst->depth_max_range = src.depth_max_range;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const DepthProperty &src, senscord_depth_property_t *dst)
  {
    dst->scale = src.scale;
    dst->depth_min_range = src.depth_min_range;
    dst->depth_max_range = src.depth_max_range;
    return Status::OK();
  }

  // ImageSensorFunctionProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_image_sensor_function_property_t &src,
      ImageSensorFunctionProperty *dst)
  {
    dst->auto_exposure = src.auto_exposure;
    dst->auto_white_balance = src.auto_white_balance;
    dst->brightness = src.brightness;
    dst->iso_sensitivity = src.iso_sensitivity;
    dst->exposure_time = src.exposure_time;
    dst->exposure_metering = src.exposure_metering;
    dst->gamma_value = src.gamma_value;
    dst->gain_value = src.gain_value;
    dst->hue = src.hue;
    dst->saturation = src.saturation;
    dst->sharpness = src.sharpness;
    dst->white_balance = src.white_balance;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImageSensorFunctionProperty &src,
      senscord_image_sensor_function_property_t *dst)
  {
    dst->auto_exposure = src.auto_exposure;
    dst->auto_white_balance = src.auto_white_balance;
    dst->brightness = src.brightness;
    dst->iso_sensitivity = src.iso_sensitivity;
    dst->exposure_time = src.exposure_time;
    StringToCharArray(src.exposure_metering, dst->exposure_metering);
    dst->gamma_value = src.gamma_value;
    dst->gain_value = src.gain_value;
    dst->hue = src.hue;
    dst->saturation = src.saturation;
    dst->sharpness = src.sharpness;
    dst->white_balance = src.white_balance;
    return Status::OK();
  }

  // ImageSensorFunctionSupportedProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_image_sensor_function_supported_property_t &src,
      ImageSensorFunctionSupportedProperty *dst)
  {
    dst->auto_exposure_supported = src.auto_exposure_supported;
    dst->auto_white_balance_supported = src.auto_white_balance_supported;
    dst->brightness_supported = src.brightness_supported;
    dst->iso_sensitivity_supported = src.iso_sensitivity_supported;
    dst->exposure_time_supported = src.exposure_time_supported;
    dst->exposure_metering_supported = src.exposure_metering_supported;
    dst->gamma_value_supported = src.gamma_value_supported;
    dst->gain_value_supported = src.gain_value_supported;
    dst->hue_supported = src.hue_supported;
    dst->saturation_supported = src.saturation_supported;
    dst->sharpness_supported = src.sharpness_supported;
    dst->white_balance_supported = src.white_balance_supported;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImageSensorFunctionSupportedProperty &src,
      senscord_image_sensor_function_supported_property_t *dst)
  {
    dst->auto_exposure_supported = src.auto_exposure_supported;
    dst->auto_white_balance_supported = src.auto_white_balance_supported;
    dst->brightness_supported = src.brightness_supported;
    dst->iso_sensitivity_supported = src.iso_sensitivity_supported;
    dst->exposure_time_supported = src.exposure_time_supported;
    dst->exposure_metering_supported = src.exposure_metering_supported;
    dst->gamma_value_supported = src.gamma_value_supported;
    dst->gain_value_supported = src.gain_value_supported;
    dst->hue_supported = src.hue_supported;
    dst->saturation_supported = src.saturation_supported;
    dst->sharpness_supported = src.sharpness_supported;
    dst->white_balance_supported = src.white_balance_supported;
    return Status::OK();
  }

  // ExposureProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_exposure_property_t &src, ExposureProperty *dst)
  {
    dst->mode = src.mode;
    dst->ev_compensation = src.ev_compensation;
    dst->exposure_time = src.exposure_time;
    dst->iso_sensitivity = src.iso_sensitivity;
    dst->metering = src.metering;
    dst->target_region.top = src.target_region.top;
    dst->target_region.left = src.target_region.left;
    dst->target_region.bottom = src.target_region.bottom;
    dst->target_region.right = src.target_region.right;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ExposureProperty &src, senscord_exposure_property_t *dst)
  {
    StringToCharArray(src.mode, dst->mode);
    dst->ev_compensation = src.ev_compensation;
    dst->exposure_time = src.exposure_time;
    dst->iso_sensitivity = src.iso_sensitivity;
    StringToCharArray(src.metering, dst->metering);
    dst->target_region.top = src.target_region.top;
    dst->target_region.left = src.target_region.left;
    dst->target_region.bottom = src.target_region.bottom;
    dst->target_region.right = src.target_region.right;
    return Status::OK();
  }

  // WhiteBalanceProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_white_balance_property_t &src, WhiteBalanceProperty *dst)
  {
    dst->mode = src.mode;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const WhiteBalanceProperty &src, senscord_white_balance_property_t *dst)
  {
    StringToCharArray(src.mode, dst->mode);
    return Status::OK();
  }

  // CameraCalibrationProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_camera_calibration_property_t &src,
      CameraCalibrationProperty *dst)
  {
    for (uint32_t i = 0; i < src.count; ++i)
    {
      const senscord_camera_calibration_parameters_t &src_params =
          src.parameters[i];
      senscord::CameraCalibrationParameters params = {};
      params.intrinsic.cx = src_params.intrinsic.cx;
      params.intrinsic.cy = src_params.intrinsic.cy;
      params.intrinsic.fx = src_params.intrinsic.fx;
      params.intrinsic.fy = src_params.intrinsic.fy;
      params.intrinsic.s = src_params.intrinsic.s;
      params.distortion.k1 = src_params.distortion.k1;
      params.distortion.k2 = src_params.distortion.k2;
      params.distortion.k3 = src_params.distortion.k3;
      params.distortion.k4 = src_params.distortion.k4;
      params.distortion.k5 = src_params.distortion.k5;
      params.distortion.k6 = src_params.distortion.k6;
      params.distortion.p1 = src_params.distortion.p1;
      params.distortion.p2 = src_params.distortion.p2;
      params.extrinsic.r11 = src_params.extrinsic.r11;
      params.extrinsic.r12 = src_params.extrinsic.r12;
      params.extrinsic.r13 = src_params.extrinsic.r13;
      params.extrinsic.r21 = src_params.extrinsic.r21;
      params.extrinsic.r22 = src_params.extrinsic.r22;
      params.extrinsic.r23 = src_params.extrinsic.r23;
      params.extrinsic.r31 = src_params.extrinsic.r31;
      params.extrinsic.r32 = src_params.extrinsic.r32;
      params.extrinsic.r33 = src_params.extrinsic.r33;
      params.extrinsic.t1 = src_params.extrinsic.t1;
      params.extrinsic.t2 = src_params.extrinsic.t2;
      params.extrinsic.t3 = src_params.extrinsic.t3;
      for (int32_t j = 0; j < 3; ++j)
      {
        for (int32_t k = 0; k < 4; ++k)
        {
          params.extrinsic.p.element[j][k] = src_params.extrinsic.p.element[j][k];
        }
      }
      dst->parameters.insert(std::make_pair(src_params.channel_id, params));
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const CameraCalibrationProperty &src,
      senscord_camera_calibration_property_t *dst)
  {
    if (src.parameters.size() > SENSCORD_CHANNEL_LIST_MAX)
    {
      dst->count = SENSCORD_CHANNEL_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.parameters.size());
    }
    std::map<uint32_t, CameraCalibrationParameters>::const_iterator
        itr = src.parameters.begin();
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      senscord_camera_calibration_parameters_t &dst_params = dst->parameters[i];
      dst_params.channel_id = itr->first;
      dst_params.intrinsic.cx = itr->second.intrinsic.cx;
      dst_params.intrinsic.cy = itr->second.intrinsic.cy;
      dst_params.intrinsic.fx = itr->second.intrinsic.fx;
      dst_params.intrinsic.fy = itr->second.intrinsic.fy;
      dst_params.intrinsic.s = itr->second.intrinsic.s;
      dst_params.distortion.k1 = itr->second.distortion.k1;
      dst_params.distortion.k2 = itr->second.distortion.k2;
      dst_params.distortion.k3 = itr->second.distortion.k3;
      dst_params.distortion.k4 = itr->second.distortion.k4;
      dst_params.distortion.k5 = itr->second.distortion.k5;
      dst_params.distortion.k6 = itr->second.distortion.k6;
      dst_params.distortion.p1 = itr->second.distortion.p1;
      dst_params.distortion.p2 = itr->second.distortion.p2;
      dst_params.extrinsic.r11 = itr->second.extrinsic.r11;
      dst_params.extrinsic.r12 = itr->second.extrinsic.r12;
      dst_params.extrinsic.r13 = itr->second.extrinsic.r13;
      dst_params.extrinsic.r21 = itr->second.extrinsic.r21;
      dst_params.extrinsic.r22 = itr->second.extrinsic.r22;
      dst_params.extrinsic.r23 = itr->second.extrinsic.r23;
      dst_params.extrinsic.r31 = itr->second.extrinsic.r31;
      dst_params.extrinsic.r32 = itr->second.extrinsic.r32;
      dst_params.extrinsic.r33 = itr->second.extrinsic.r33;
      dst_params.extrinsic.t1 = itr->second.extrinsic.t1;
      dst_params.extrinsic.t2 = itr->second.extrinsic.t2;
      dst_params.extrinsic.t3 = itr->second.extrinsic.t3;
      for (int32_t j = 0; j < 3; ++j)
      {
        for (int32_t k = 0; k < 4; ++k)
        {
          dst_params.extrinsic.p.element[j][k] =
              itr->second.extrinsic.p.element[j][k];
        }
      }
      ++itr;
    }
    return Status::OK();
  }

  // InterlaceProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_interlace_property_t &src, InterlaceProperty *dst)
  {
    dst->field = static_cast<InterlaceField>(src.field);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const InterlaceProperty &src, senscord_interlace_property_t *dst)
  {
    dst->field = static_cast<senscord_interlace_field_t>(src.field);
    return Status::OK();
  }

  // InterlaceInfoProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_interlace_info_property_t &src,
      InterlaceInfoProperty *dst)
  {
    dst->order = static_cast<senscord::InterlaceOrder>(src.order);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const InterlaceInfoProperty &src,
      senscord_interlace_info_property_t *dst)
  {
    dst->order = static_cast<senscord_interlace_order_t>(src.order);
    return Status::OK();
  }

  // BaselineLengthProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_base_line_length_property_t &src,
      BaselineLengthProperty *dst)
  {
    dst->length_mm = src.length_mm;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const BaselineLengthProperty &src,
      senscord_base_line_length_property_t *dst)
  {
    dst->length_mm = src.length_mm;
    return Status::OK();
  }

  // ImuDataUnitProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_imu_data_unit_property_t &src, ImuDataUnitProperty *dst)
  {
    dst->acceleration = static_cast<AccelerationUnit>(src.acceleration);
    dst->angular_velocity = static_cast<AngularVelocityUnit>(
        src.angular_velocity);
    dst->magnetic_field = static_cast<MagneticFieldUnit>(src.magnetic_field);
    dst->orientation = static_cast<OrientationUnit>(src.orientation);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ImuDataUnitProperty &src, senscord_imu_data_unit_property_t *dst)
  {
    dst->acceleration =
        static_cast<senscord_acceleration_unit_t>(src.acceleration);
    dst->angular_velocity =
        static_cast<senscord_angular_velocity_unit_t>(src.angular_velocity);
    dst->magnetic_field =
        static_cast<senscord_magnetic_field_unit_t>(src.magnetic_field);
    dst->orientation = static_cast<senscord_orientation_unit_t>(src.orientation);
    return Status::OK();
  }

  // MagneticNorthCalibProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_magnetic_north_calib_property_t &src,
      MagneticNorthCalibProperty *dst)
  {
    dst->declination = src.declination;
    dst->inclination = src.inclination;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const MagneticNorthCalibProperty &src,
      senscord_magnetic_north_calib_property_t *dst)
  {
    dst->declination = src.declination;
    dst->inclination = src.inclination;
    return Status::OK();
  }

  // SlamDataSupportedProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_slam_data_supported_property_t &src,
      SlamDataSupportedProperty *dst)
  {
    dst->odometry_supported = src.odometry_supported;
    dst->gridmap_supported = src.gridmap_supported;
    dst->pointcloud_supported = src.pointcloud_supported;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const SlamDataSupportedProperty &src,
      senscord_slam_data_supported_property_t *dst)
  {
    dst->odometry_supported = src.odometry_supported;
    dst->gridmap_supported = src.gridmap_supported;
    dst->pointcloud_supported = src.pointcloud_supported;
    return Status::OK();
  }

  // PoseDataProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_pose_data_property_t &src, PoseDataProperty *dst)
  {
    dst->data_format = src.data_format;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PoseDataProperty &src, senscord_pose_data_property_t *dst)
  {
    StringToCharArray(src.data_format, dst->data_format);
    return Status::OK();
  }

  // OdometryDataProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_odometry_data_property_t &src, OdometryDataProperty *dst)
  {
    dst->coordinate_system =
        static_cast<CoordinateSystem>(src.coordinate_system);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const OdometryDataProperty &src, senscord_odometry_data_property_t *dst)
  {
    dst->coordinate_system =
        static_cast<senscord_coordinate_system_t>(src.coordinate_system);
    return Status::OK();
  }

  // GridSizeProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_grid_size_property_t &src, GridSizeProperty *dst)
  {
    dst->x = src.x;
    dst->y = src.y;
    dst->z = src.z;
    dst->unit = static_cast<GridUnit>(src.unit);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const GridSizeProperty &src, senscord_grid_size_property_t *dst)
  {
    dst->x = src.x;
    dst->y = src.y;
    dst->z = src.z;
    dst->unit = static_cast<senscord_grid_unit_t>(src.unit);
    return Status::OK();
  }

  // GridMapProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_grid_map_property_t &src, GridMapProperty *dst)
  {
    dst->grid_num_x = src.grid_num_x;
    dst->grid_num_y = src.grid_num_y;
    dst->grid_num_z = src.grid_num_z;
    dst->pixel_format = src.pixel_format;
    dst->grid_size.x = src.grid_size.x;
    dst->grid_size.y = src.grid_size.y;
    dst->grid_size.z = src.grid_size.z;
    dst->grid_size.unit = static_cast<GridUnit>(src.grid_size.unit);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const GridMapProperty &src, senscord_grid_map_property_t *dst)
  {
    dst->grid_num_x = src.grid_num_x;
    dst->grid_num_y = src.grid_num_y;
    dst->grid_num_z = src.grid_num_z;
    StringToCharArray(src.pixel_format, dst->pixel_format);
    dst->grid_size.x = src.grid_size.x;
    dst->grid_size.y = src.grid_size.y;
    dst->grid_size.z = src.grid_size.z;
    dst->grid_size.unit = static_cast<senscord_grid_unit_t>(src.grid_size.unit);
    return Status::OK();
  }

  // PointCloudProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_point_cloud_property_t &src, PointCloudProperty *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    dst->pixel_format = src.pixel_format;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PointCloudProperty &src, senscord_point_cloud_property_t *dst)
  {
    dst->width = src.width;
    dst->height = src.height;
    StringToCharArray(src.pixel_format, dst->pixel_format);
    return Status::OK();
  }

  // RegisterAccess64Property
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_register_access_64_property_t &src,
      RegisterAccess64Property *dst)
  {
    dst->id = src.id;
    RegisterAccessElement<uint64_t> element = {};
    element.address = src.address;
    element.data = src.data;
    dst->element.push_back(element);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RegisterAccess64Property &src,
      senscord_register_access_64_property_t *dst)
  {
    dst->id = src.id;
    if (!src.element.empty())
    {
      dst->address = src.element[0].address;
      dst->data = src.element[0].data;
    }
    else
    {
      dst->address = 0;
      dst->data = 0;
    }
    return Status::OK();
  }

  // RegisterAccess32Property
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_register_access_32_property_t &src,
      RegisterAccess32Property *dst)
  {
    dst->id = src.id;
    RegisterAccessElement<uint32_t> element = {};
    element.address = src.address;
    element.data = src.data;
    dst->element.push_back(element);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RegisterAccess32Property &src,
      senscord_register_access_32_property_t *dst)
  {
    dst->id = src.id;
    if (!src.element.empty())
    {
      dst->address = src.element[0].address;
      dst->data = src.element[0].data;
    }
    else
    {
      dst->address = 0;
      dst->data = 0;
    }
    return Status::OK();
  }

  // RegisterAccess16Property
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_register_access_16_property_t &src,
      RegisterAccess16Property *dst)
  {
    dst->id = src.id;
    RegisterAccessElement<uint16_t> element = {};
    element.address = src.address;
    element.data = src.data;
    dst->element.push_back(element);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RegisterAccess16Property &src,
      senscord_register_access_16_property_t *dst)
  {
    dst->id = src.id;
    if (!src.element.empty())
    {
      dst->address = src.element[0].address;
      dst->data = src.element[0].data;
    }
    else
    {
      dst->address = 0;
      dst->data = 0;
    }
    return Status::OK();
  }

  // RegisterAccess8Property
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_register_access_8_property_t &src,
      RegisterAccess8Property *dst)
  {
    dst->id = src.id;
    RegisterAccessElement<uint8_t> element = {};
    element.address = src.address;
    element.data = src.data;
    dst->element.push_back(element);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RegisterAccess8Property &src,
      senscord_register_access_8_property_t *dst)
  {
    dst->id = src.id;
    if (!src.element.empty())
    {
      dst->address = src.element[0].address;
      dst->data = src.element[0].data;
    }
    else
    {
      dst->address = 0;
      dst->data = 0;
    }
    return Status::OK();
  }

  // TemperatureProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_temperature_property_t &src, TemperatureProperty *dst)
  {
    for (uint32_t i = 0; i < src.count; ++i)
    {
      senscord::TemperatureInfo info = {};
      info.temperature = src.temperatures[i].temperature;
      info.description = src.temperatures[i].description;
      dst->temperatures.insert(
          std::make_pair(src.temperatures[i].sensor_id, info));
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const TemperatureProperty &src, senscord_temperature_property_t *dst)
  {
    if (src.temperatures.size() > SENSCORD_TEMPERATURE_LIST_MAX)
    {
      dst->count = SENSCORD_TEMPERATURE_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.temperatures.size());
    }
    std::map<uint32_t, TemperatureInfo>::const_iterator itr =
        src.temperatures.begin();
    for (uint32_t i = 0; i < dst->count; ++i)
    {
      senscord_temperature_info_t &dst_info = dst->temperatures[i];
      dst_info.sensor_id = itr->first;
      dst_info.temperature = itr->second.temperature;
      StringToCharArray(itr->second.description, dst_info.description);
      ++itr;
    }
    return Status::OK();
  }

  // PolarizationDopCorrectionProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_polarization_dop_correction_property_t &src,
      PolarizationDopCorrectionProperty *dst)
  {
    dst->noise_model = src.noise_model;
    dst->analog_gain = src.analog_gain;
    dst->dop_gain = src.dop_gain;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PolarizationDopCorrectionProperty &src,
      senscord_polarization_dop_correction_property_t *dst)
  {
    dst->noise_model = src.noise_model;
    dst->analog_gain = src.analog_gain;
    dst->dop_gain = src.dop_gain;
    return Status::OK();
  }

  // PolarizationInvalidMaskProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_polarization_invalid_mask_property_t &src,
      PolarizationInvalidMaskProperty *dst)
  {
    dst->enable = src.enable;
    dst->pixel_black_threshold = src.pixel_black_threshold;
    dst->pixel_white_threshold = src.pixel_white_threshold;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PolarizationInvalidMaskProperty &src,
      senscord_polarization_invalid_mask_property_t *dst)
  {
    dst->enable = src.enable;
    dst->pixel_black_threshold = src.pixel_black_threshold;
    dst->pixel_white_threshold = src.pixel_white_threshold;
    return Status::OK();
  }

  // PolarizationNormalVectorProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_polarization_normal_vector_property_t &src,
      PolarizationNormalVectorProperty *dst)
  {
    dst->color_type = static_cast<ColorType>(src.color_type);
    dst->rotation = src.rotation;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PolarizationNormalVectorProperty &src,
      senscord_polarization_normal_vector_property_t *dst)
  {
    dst->color_type = static_cast<senscord_color_type_t>(src.color_type);
    dst->rotation = src.rotation;
    return Status::OK();
  }

  // PolarizationReflectionProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_polarization_reflection_property_t &src,
      PolarizationReflectionProperty *dst)
  {
    dst->extraction_gain = src.extraction_gain;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const PolarizationReflectionProperty &src,
      senscord_polarization_reflection_property_t *dst)
  {
    dst->extraction_gain = src.extraction_gain;
    return Status::OK();
  }

  // TemporalContrastDataProperty (PixelPolarityDataProperty)
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_temporal_contrast_data_property_t &src,
      TemporalContrastDataProperty *dst)
  {
    dst->trigger_type =
        static_cast<senscord::TemporalContrastTriggerType>(src.trigger_type);
    dst->event_count = src.event_count;
    dst->accumulation_time = src.accumulation_time;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const TemporalContrastDataProperty &src,
      senscord_temporal_contrast_data_property_t *dst)
  {
    dst->trigger_type =
        static_cast<senscord_temporal_contrast_trigger_type_t>(src.trigger_type);
    dst->event_count = src.event_count;
    dst->accumulation_time = src.accumulation_time;
    return Status::OK();
  }

  // RoiProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_roi_property_t &src,
      RoiProperty *dst)
  {
    dst->top = src.top;
    dst->left = src.left;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RoiProperty &src,
      senscord_roi_property_t *dst)
  {
    dst->top = src.top;
    dst->left = src.left;
    dst->width = src.width;
    dst->height = src.height;
    return Status::OK();
  }

  // ScoreThresholdProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_score_threshold_property_t &src,
      ScoreThresholdProperty *dst)
  {
    dst->score_threshold = src.score_threshold;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const ScoreThresholdProperty &src,
      senscord_score_threshold_property_t *dst)
  {
    dst->score_threshold = src.score_threshold;
    return Status::OK();
  }

  // VelocityDataUnitProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_velocity_data_unit_property_t &src, VelocityDataUnitProperty *dst)
  {
    dst->velocity = static_cast<VelocityUnit>(src.velocity);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const VelocityDataUnitProperty &src, senscord_velocity_data_unit_property_t *dst)
  {
    dst->velocity =
        static_cast<senscord_velocity_unit_t>(src.velocity);
    return Status::OK();
  }

  // DataRateProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_data_rate_property_t &src, DataRateProperty *dst)
  {
    for (uint32_t i = 0; i < src.count; ++i)
    {
      DataRateElement element = {};
      element.size = src.elements[i].size;
      element.name = src.elements[i].name;
      element.unit = src.elements[i].unit;
      dst->elements.push_back(element);
    }
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const DataRateProperty &src, senscord_data_rate_property_t *dst)
  {
    if (src.elements.size() > SENSCORD_DATA_RATE_ELEMENT_LIST_MAX)
    {
      dst->count = SENSCORD_DATA_RATE_ELEMENT_LIST_MAX;
    }
    else
    {
      dst->count = static_cast<uint32_t>(src.elements.size());
    }

    for (uint32_t i = 0; i < dst->count; i++)
    {
      dst->elements[i].size = src.elements[i].size;
      StringToCharArray(src.elements[i].name, dst->elements[i].name);
      StringToCharArray(src.elements[i].unit, dst->elements[i].unit);
    }
    return Status::OK();
  }

  // RotationData
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_rotation_data_t &src, RotationData *dst)
  {
    dst->roll = src.roll;
    dst->pitch = src.pitch;
    dst->yaw = src.yaw;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const RotationData &src, senscord_rotation_data_t *dst)
  {
    dst->roll = src.roll;
    dst->pitch = src.pitch;
    dst->yaw = src.yaw;
    return Status::OK();
  }

#ifdef SENSCORD_SERIALIZE
  /**
   * @brief Serialize the pose data.
   * @param[in]  input_data   Pointer to the input data.
   * @param[in]  input_size   Size of the input data.
   * @param[out] output_data  Output destination container.
   * @return Status object.
   */
  Status PoseDataConverter::Serialize(
      const void *input_data, size_t input_size,
      std::vector<uint8_t> *output_data)
  {
    if (input_size != sizeof(senscord_pose_quaternion_data_t) &&
        input_size != sizeof(senscord_pose_matrix_data_t))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "invalid input size.");
    }

    Status status;
    serialize::SerializedBuffer buffer;
    serialize::Encoder encoder(&buffer);

    if (input_size == sizeof(senscord_pose_quaternion_data_t))
    {
      // Quaternion
      const senscord_pose_quaternion_data_t *src =
          reinterpret_cast<const senscord_pose_quaternion_data_t *>(input_data);
      PoseQuaternionData tmp = {};
      status = c_to_cxx(*src, &tmp);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok())
      {
        status = encoder.Push(tmp);
        SENSCORD_STATUS_TRACE(status);
      }
    }
    else
    {
      // Matrix
      const senscord_pose_matrix_data_t *src =
          reinterpret_cast<const senscord_pose_matrix_data_t *>(input_data);
      PoseMatrixData tmp = {};
      status = c_to_cxx(*src, &tmp);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok())
      {
        status = encoder.Push(tmp);
        SENSCORD_STATUS_TRACE(status);
      }
    }

    if (status.ok())
    {
      status = buffer.swap(output_data);
      SENSCORD_STATUS_TRACE(status);
    }

    return status;
  }

  /**
   * @brief Deserialize the pose data.
   * @param[in]  input_data   Pointer to the input data.
   * @param[in]  input_size   Size of the input data.
   * @param[out] output_data  Pointer to the output data.
   * @param[in]  output_size  Size of the output data.
   * @return Status object.
   */
  Status PoseDataConverter::Deserialize(
      const void *input_data, size_t input_size,
      void *output_data, size_t output_size)
  {
    if (output_size != sizeof(senscord_pose_quaternion_data_t) &&
        output_size != sizeof(senscord_pose_matrix_data_t))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "invalid output size.");
    }

    Status status;
    serialize::Decoder decoder(input_data, input_size);

    if (output_size == sizeof(senscord_pose_quaternion_data_t))
    {
      // Quaternion
      PoseQuaternionData tmp = {};
      status = decoder.Pop(tmp);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok())
      {
        senscord_pose_quaternion_data_t *dst =
            reinterpret_cast<senscord_pose_quaternion_data_t *>(output_data);
        status = cxx_to_c(tmp, dst);
        SENSCORD_STATUS_TRACE(status);
      }
    }
    else
    {
      // Matrix
      PoseMatrixData tmp = {};
      status = decoder.Pop(tmp);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok())
      {
        senscord_pose_matrix_data_t *dst =
            reinterpret_cast<senscord_pose_matrix_data_t *>(output_data);
        status = cxx_to_c(tmp, dst);
        SENSCORD_STATUS_TRACE(status);
      }
    }

    return status;
  }
#else
  /**
   * @brief Creates the C++ property.
   * @param[in]  input_data       Pointer to the C property.
   * @param[in]  input_size       Size of the C property.
   * @param[out] output_property  Output C++ property instance.
   * @return Status object.
   */
  Status PoseDataConverter::CreateCxxProperty(
      const void *input_data, size_t input_size,
      void **output_property)
  {
    if (input_size != sizeof(senscord_pose_quaternion_data_t) &&
        input_size != sizeof(senscord_pose_matrix_data_t))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "invalid input size.");
    }
    Status status;
    if (input_size == sizeof(senscord_pose_quaternion_data_t))
    {
      // Quaternion
      const senscord_pose_quaternion_data_t *src =
          reinterpret_cast<const senscord_pose_quaternion_data_t *>(input_data);
      PoseQuaternionData *tmp = new PoseQuaternionData();
      status = c_to_cxx(*src, tmp);
      if (!status.ok())
      {
        delete tmp;
        return SENSCORD_STATUS_TRACE(status);
      }
      *output_property = tmp;
    }
    else
    {
      // Matrix
      const senscord_pose_matrix_data_t *src =
          reinterpret_cast<const senscord_pose_matrix_data_t *>(input_data);
      PoseMatrixData *tmp = new PoseMatrixData();
      status = c_to_cxx(*src, tmp);
      if (!status.ok())
      {
        delete tmp;
        return SENSCORD_STATUS_TRACE(status);
      }
      *output_property = tmp;
    }
    return status;
  }

  /**
   * @brief Deletes the C++ property.
   * @param[in]  input_data  Pointer to the C property.
   * @param[in]  input_size  Size of the C property.
   * @param[in]  property    C++ property instance.
   * @return Status object.
   */
  void PoseDataConverter::DeleteCxxProperty(
      const void *input_data, size_t input_size,
      void *property)
  {
    if (input_size == sizeof(senscord_pose_quaternion_data_t))
    {
      delete reinterpret_cast<PoseQuaternionData *>(property);
    }
    else if (input_size == sizeof(senscord_pose_matrix_data_t))
    {
      delete reinterpret_cast<PoseMatrixData *>(property);
    }
  }

  /**
   * @brief Converts the property.
   * @param[in]  input_property  Pointer to the C++ property.
   * @param[out] output_data     Pointer to the C property.
   * @param[in]  output_size     Size of the C property.
   * @return Status object.
   */
  Status PoseDataConverter::ConvertProperty(
      const void *input_property,
      void *output_data, size_t output_size)
  {
    if (output_size != sizeof(senscord_pose_quaternion_data_t) &&
        output_size != sizeof(senscord_pose_matrix_data_t))
    {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "invalid output size.");
    }
    Status status;
    if (output_size == sizeof(senscord_pose_quaternion_data_t))
    {
      // Quaternion
      const PoseQuaternionData *src =
          reinterpret_cast<const PoseQuaternionData *>(input_property);
      senscord_pose_quaternion_data_t *dst =
          reinterpret_cast<senscord_pose_quaternion_data_t *>(output_data);
      status = cxx_to_c(*src, dst);
      SENSCORD_STATUS_TRACE(status);
    }
    else
    {
      // Matrix
      const PoseMatrixData *src =
          reinterpret_cast<const PoseMatrixData *>(input_property);
      senscord_pose_matrix_data_t *dst =
          reinterpret_cast<senscord_pose_matrix_data_t *>(output_data);
      status = cxx_to_c(*src, dst);
    }
    return status;
  }
#endif // SENSCORD_SERIALIZE

  // PoseQuaternionData
  Status PoseDataConverter::c_to_cxx(
      const senscord_pose_quaternion_data_t &src, PoseQuaternionData *dst)
  {
    dst->position.x = src.position.x;
    dst->position.y = src.position.y;
    dst->position.z = src.position.z;
    dst->orientation.x = src.orientation.x;
    dst->orientation.y = src.orientation.y;
    dst->orientation.z = src.orientation.z;
    dst->orientation.w = src.orientation.w;
    return Status::OK();
  }

  Status PoseDataConverter::cxx_to_c(
      const PoseQuaternionData &src, senscord_pose_quaternion_data_t *dst)
  {
    dst->position.x = src.position.x;
    dst->position.y = src.position.y;
    dst->position.z = src.position.z;
    dst->orientation.x = src.orientation.x;
    dst->orientation.y = src.orientation.y;
    dst->orientation.z = src.orientation.z;
    dst->orientation.w = src.orientation.w;
    return Status::OK();
  }

  // PoseMatrixData
  Status PoseDataConverter::c_to_cxx(
      const senscord_pose_matrix_data_t &src, PoseMatrixData *dst)
  {
    dst->position.x = src.position.x;
    dst->position.y = src.position.y;
    dst->position.z = src.position.z;
    dst->rotation.element[0][0] = src.rotation.element[0][0];
    dst->rotation.element[0][1] = src.rotation.element[0][1];
    dst->rotation.element[0][2] = src.rotation.element[0][2];
    dst->rotation.element[1][0] = src.rotation.element[1][0];
    dst->rotation.element[1][1] = src.rotation.element[1][1];
    dst->rotation.element[1][2] = src.rotation.element[1][2];
    dst->rotation.element[2][0] = src.rotation.element[2][0];
    dst->rotation.element[2][1] = src.rotation.element[2][1];
    dst->rotation.element[2][2] = src.rotation.element[2][2];
    return Status::OK();
  }

  Status PoseDataConverter::cxx_to_c(
      const PoseMatrixData &src, senscord_pose_matrix_data_t *dst)
  {
    dst->position.x = src.position.x;
    dst->position.y = src.position.y;
    dst->position.z = src.position.z;
    dst->rotation.element[0][0] = src.rotation.element[0][0];
    dst->rotation.element[0][1] = src.rotation.element[0][1];
    dst->rotation.element[0][2] = src.rotation.element[0][2];
    dst->rotation.element[1][0] = src.rotation.element[1][0];
    dst->rotation.element[1][1] = src.rotation.element[1][1];
    dst->rotation.element[1][2] = src.rotation.element[1][2];
    dst->rotation.element[2][0] = src.rotation.element[2][0];
    dst->rotation.element[2][1] = src.rotation.element[2][1];
    dst->rotation.element[2][2] = src.rotation.element[2][2];
    return Status::OK();
  }

  // CoordinateSystemProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_coordinate_system_property_t &src,
      CoordinateSystemProperty *dst)
  {
    dst->handed = static_cast<SystemHanded>(src.handed);
    dst->up_axis = static_cast<UpAxis>(src.up_axis);
    dst->forward_axis = static_cast<ForwardAxis>(src.forward_axis);
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const CoordinateSystemProperty &src,
      senscord_coordinate_system_property_t *dst)
  {
    dst->handed = static_cast<senscord_system_handed_t>(src.handed);
    dst->up_axis = static_cast<senscord_up_axis_t>(src.up_axis);
    dst->forward_axis = static_cast<senscord_forward_axis_t>(src.forward_axis);
    return Status::OK();
  }

  // AudioProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_audio_property_t &src, AudioProperty *dst)
  {
    dst->format = src.format;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const AudioProperty &src, senscord_audio_property_t *dst)
  {
    StringToCharArray(src.format, dst->format);
    return Status::OK();
  }

  // AudioPcmProperty
  Status CoreConverterLibrary::c_to_cxx(
      const senscord_audio_pcm_property_t &src, AudioPcmProperty *dst)
  {
    dst->channels = src.channels;
    dst->interleaved = src.interleaved;
    dst->format = static_cast<AudioPcm::Format>(src.format);
    dst->samples_per_second = src.samples_per_second;
    dst->samples_per_frame = src.samples_per_frame;
    return Status::OK();
  }

  Status CoreConverterLibrary::cxx_to_c(
      const AudioPcmProperty &src, senscord_audio_pcm_property_t *dst)
  {
    dst->channels = src.channels;
    dst->interleaved = src.interleaved;
    dst->format = static_cast<senscord_audio_pcm_format_t>(src.format);
    dst->samples_per_second = src.samples_per_second;
    dst->samples_per_frame = src.samples_per_frame;
    return Status::OK();
  }

} // namespace senscord
