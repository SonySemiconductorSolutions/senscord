/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CORE_CONVERTER_SRC_CORE_CONVERTER_H_
#define CORE_CONVERTER_SRC_CORE_CONVERTER_H_

#include <stdint.h>
#include <vector>

#include "senscord/develop/converter.h"
#include "senscord/property_types.h"
#include "senscord/c_api/property_c_types.h"
#include "senscord/c_api/rawdata_c_types.h"

// Macro for defining conversion function.
#define CONVERTER_FUNC(c, cxx)                               \
  virtual senscord::Status c_to_cxx(const c& src, cxx* dst); \
  virtual senscord::Status cxx_to_c(const cxx& src, c* dst)

namespace senscord {

/**
 * @brief Special converter for PoseData (Quaternion / Matrix).
 */
class PoseDataConverter : public ConverterBase {
 public:
  virtual ~PoseDataConverter() {}

#ifdef SENSCORD_SERIALIZE
  /**
   * @brief Serialize the pose data.
   * @param[in]  input_data   Pointer to the input data.
   * @param[in]  input_size   Size of the input data.
   * @param[out] output_data  Output destination container.
   * @return Status object.
   */
  virtual Status Serialize(
      const void* input_data, size_t input_size,
      std::vector<uint8_t>* output_data);

  /**
   * @brief Deserialize the pose data.
   * @param[in]  input_data   Pointer to the input data.
   * @param[in]  input_size   Size of the input data.
   * @param[out] output_data  Pointer to the output data.
   * @param[in]  output_size  Size of the output data.
   * @return Status object.
   */
  virtual Status Deserialize(
      const void* input_data, size_t input_size,
      void* output_data, size_t output_size);
#else
  /**
   * @brief Creates the C++ property.
   * @param[in]  input_data       Pointer to the C property.
   * @param[in]  input_size       Size of the C property.
   * @param[out] output_property  Output C++ property instance.
   * @return Status object.
   */
  virtual Status CreateCxxProperty(
      const void* input_data, size_t input_size,
      void** output_property);

  /**
   * @brief Deletes the C++ property.
   * @param[in]  input_data  Pointer to the C property.
   * @param[in]  input_size  Size of the C property.
   * @param[in]  property    C++ property instance.
   * @return Status object.
   */
  virtual void DeleteCxxProperty(
      const void* input_data, size_t input_size,
      void* property);

  /**
   * @brief Converts the property.
   * @param[in]  input_property  Pointer to the C++ property.
   * @param[out] output_data     Pointer to the C property.
   * @param[in]  output_size     Size of the C property.
   * @return Status object.
   */
  virtual Status ConvertProperty(
      const void* input_property,
      void* output_data, size_t output_size);
#endif  // SENSCORD_SERIALIZE

 private:
  // PoseQuaternionData
  CONVERTER_FUNC(senscord_pose_quaternion_data_t, PoseQuaternionData);

  // PoseMatrixData
  CONVERTER_FUNC(senscord_pose_matrix_data_t, PoseMatrixData);
};

/**
 * @brief Core converter library.
 */
class CoreConverterLibrary
  : public ConverterLibrary,
    // Common
    public StructConverterC<senscord_scalar_f_t, Scalar<float> >,
    public StructConverterC<senscord_vector3f_t, Vector3<float> >,
    public StructConverterC<senscord_axis_misalignment_t, AxisMisalignment>,
    // Property
    public StructConverterC<senscord_version_property_t, VersionProperty>,
    public StructConverterC<senscord_stream_type_property_t, StreamTypeProperty>,
    public StructConverterC<senscord_stream_key_property_t, StreamKeyProperty>,
    public StructConverterC<senscord_stream_state_property_t, StreamStateProperty>,
    public StructConverterC<senscord_frame_buffering_property_t, FrameBufferingProperty>,
    public StructConverterC<senscord_current_frame_num_property_t, CurrentFrameNumProperty>,
    public StructConverterC<senscord_channel_info_property_t, ChannelInfoProperty>,
    public StructConverterC<senscord_channel_mask_property_t, ChannelMaskProperty>,
#ifdef SENSCORD_RECORDER
    public StructConverterC<senscord_record_property_t, RecordProperty>,
    public StructConverterC<senscord_recorder_list_property_t, RecorderListProperty>,
#endif  // SENSCORD_RECORDER
#ifdef SENSCORD_PLAYER
    public StructConverterC<senscord_play_file_info_property_t, PlayFileInfoProperty>,
    public StructConverterC<senscord_play_mode_property_t, PlayModeProperty>,
    public StructConverterC<senscord_play_property_t, PlayProperty>,
    public StructConverterC<senscord_play_position_property_t, PlayPositionProperty>,
#endif  // SENSCORD_PLAYER
    public StructConverterC<senscord_preset_list_property_t, PresetListProperty>,
    public StructConverterC<senscord_preset_property_t, PresetProperty>,
    public StructConverterC<senscord_image_property_t, ImageProperty>,
    public StructConverterC<senscord_image_crop_property_t, ImageCropProperty>,
    public StructConverterC<senscord_image_crop_bounds_property_t, ImageCropBoundsProperty>,
    public StructConverterC<senscord_confidence_property_t, ConfidenceProperty>,
    public StructConverterC<senscord_color_space_property_t, ColorSpaceProperty>,
    public StructConverterC<senscord_frame_rate_property_t, FrameRateProperty>,
    public StructConverterC<senscord_skip_frame_property_t, SkipFrameProperty>,
    public StructConverterC<senscord_lens_property_t, LensProperty>,
    public StructConverterC<senscord_depth_property_t, DepthProperty>,
    public StructConverterC<senscord_image_sensor_function_property_t, ImageSensorFunctionProperty>,
    public StructConverterC<senscord_image_sensor_function_supported_property_t, ImageSensorFunctionSupportedProperty>,
    public StructConverterC<senscord_exposure_property_t, ExposureProperty>,
    public StructConverterC<senscord_white_balance_property_t, WhiteBalanceProperty>,
    public StructConverterC<senscord_camera_calibration_property_t, CameraCalibrationProperty>,
    public StructConverterC<senscord_interlace_property_t, InterlaceProperty>,
    public StructConverterC<senscord_interlace_info_property_t, InterlaceInfoProperty>,
    public StructConverterC<senscord_base_line_length_property_t, BaselineLengthProperty>,
    public StructConverterC<senscord_imu_data_unit_property_t, ImuDataUnitProperty>,
    public StructConverterC<senscord_magnetic_north_calib_property_t, MagneticNorthCalibProperty>,
    public StructConverterC<senscord_slam_data_supported_property_t, SlamDataSupportedProperty>,
    public StructConverterC<senscord_pose_data_property_t, PoseDataProperty>,
    public StructConverterC<senscord_odometry_data_property_t, OdometryDataProperty>,
    public StructConverterC<senscord_grid_size_property_t, GridSizeProperty>,
    public StructConverterC<senscord_grid_map_property_t, GridMapProperty>,
    public StructConverterC<senscord_point_cloud_property_t, PointCloudProperty>,
    public StructConverterC<senscord_register_access_64_property_t, RegisterAccess64Property>,
    public StructConverterC<senscord_register_access_32_property_t, RegisterAccess32Property>,
    public StructConverterC<senscord_register_access_16_property_t, RegisterAccess16Property>,
    public StructConverterC<senscord_register_access_8_property_t, RegisterAccess8Property>,
    public StructConverterC<senscord_temperature_property_t, TemperatureProperty>,
    public StructConverterC<senscord_polarization_dop_correction_property_t, PolarizationDopCorrectionProperty>,
    public StructConverterC<senscord_polarization_invalid_mask_property_t, PolarizationInvalidMaskProperty>,
    public StructConverterC<senscord_polarization_normal_vector_property_t, PolarizationNormalVectorProperty>,
    public StructConverterC<senscord_polarization_reflection_property_t, PolarizationReflectionProperty>,
    public StructConverterC<senscord_temporal_contrast_data_property_t, TemporalContrastDataProperty>,
    public StructConverterC<senscord_roi_property_t, RoiProperty>,
    public StructConverterC<senscord_score_threshold_property_t, ScoreThresholdProperty>,
    public StructConverterC<senscord_velocity_data_unit_property_t, VelocityDataUnitProperty>,
    public StructConverterC<senscord_data_rate_property_t , DataRateProperty >,
    public StructConverterC<senscord_coordinate_system_property_t , CoordinateSystemProperty>,
    public StructConverterC<senscord_audio_property_t , AudioProperty>,
    public StructConverterC<senscord_audio_pcm_property_t , AudioPcmProperty>,
    // RawData
    public StructConverterC<senscord_rotation_data_t, RotationData>,
    public PoseDataConverter {
 public:
  virtual ~CoreConverterLibrary() {}

  /**
   * @brief Initialize the converter library.
   * @param[in] collector  Converter collector.
   * @return Status object.
   */
  virtual Status Init(ConverterCollector* collector);

 private:
  /* Common */
  // Scalar<float>
  CONVERTER_FUNC(senscord_scalar_f_t, Scalar<float>);

  // Vector3<float>
  CONVERTER_FUNC(senscord_vector3f_t, Vector3<float>);

  // AxisMisalignment
  CONVERTER_FUNC(senscord_axis_misalignment_t, AxisMisalignment);

  /* Property */
  // VersionProperty
  CONVERTER_FUNC(senscord_version_property_t, VersionProperty);

  // StreamTypeProperty
  CONVERTER_FUNC(senscord_stream_type_property_t, StreamTypeProperty);

  // StreamKeyProperty
  CONVERTER_FUNC(senscord_stream_key_property_t, StreamKeyProperty);

  // StreamStateProperty
  CONVERTER_FUNC(senscord_stream_state_property_t, StreamStateProperty);

  // FrameBufferingProperty
  CONVERTER_FUNC(senscord_frame_buffering_property_t, FrameBufferingProperty);

  // CurrentFrameNumProperty
  CONVERTER_FUNC(senscord_current_frame_num_property_t,
                 CurrentFrameNumProperty);

  // ChannelInfoProperty
  CONVERTER_FUNC(senscord_channel_info_property_t, ChannelInfoProperty);

  // ChannelMaskProperty
  CONVERTER_FUNC(senscord_channel_mask_property_t, ChannelMaskProperty);

#ifdef SENSCORD_RECORDER
  // RecordProperty
  CONVERTER_FUNC(senscord_record_property_t, RecordProperty);

  // RecorderListProperty
  CONVERTER_FUNC(senscord_recorder_list_property_t, RecorderListProperty);
#endif  // SENSCORD_RECORDER

#ifdef SENSCORD_PLAYER
  // PlayFileInfoProperty
  CONVERTER_FUNC(senscord_play_file_info_property_t, PlayFileInfoProperty);

  // PlayModeProperty
  CONVERTER_FUNC(senscord_play_mode_property_t, PlayModeProperty);

  // PlayProperty
  CONVERTER_FUNC(senscord_play_property_t, PlayProperty);

  // PlayProperty
  CONVERTER_FUNC(senscord_play_position_property_t, PlayPositionProperty);
#endif  // SENSCORD_PLAYER

  // PresetListProperty
  CONVERTER_FUNC(senscord_preset_list_property_t, PresetListProperty);

  // PresetProperty
  CONVERTER_FUNC(senscord_preset_property_t, PresetProperty);

  // ImageProperty
  CONVERTER_FUNC(senscord_image_property_t, ImageProperty);

  // ImageCropProperty
  CONVERTER_FUNC(senscord_image_crop_property_t, ImageCropProperty);

  // ImageCropBoundsProperty
  CONVERTER_FUNC(senscord_image_crop_bounds_property_t,
                 ImageCropBoundsProperty);

  // ConfidenceProperty
  CONVERTER_FUNC(senscord_confidence_property_t, ConfidenceProperty);

  // ColorSpaceProperty
  CONVERTER_FUNC(senscord_color_space_property_t, ColorSpaceProperty);

  // FrameRateProperty
  CONVERTER_FUNC(senscord_frame_rate_property_t, FrameRateProperty);

  // SkipFrameProperty
  CONVERTER_FUNC(senscord_skip_frame_property_t, SkipFrameProperty);

  // LensProperty
  CONVERTER_FUNC(senscord_lens_property_t, LensProperty);

  // DepthProperty
  CONVERTER_FUNC(senscord_depth_property_t, DepthProperty);

  // ImageSensorFunctionProperty
  CONVERTER_FUNC(senscord_image_sensor_function_property_t,
                 ImageSensorFunctionProperty);

  // ImageSensorFunctionSupportedProperty
  CONVERTER_FUNC(senscord_image_sensor_function_supported_property_t,
                 ImageSensorFunctionSupportedProperty);

  // ExposureProperty
  CONVERTER_FUNC(senscord_exposure_property_t, ExposureProperty);

  // WhiteBalanceProperty
  CONVERTER_FUNC(senscord_white_balance_property_t, WhiteBalanceProperty);

  // CameraCalibrationProperty
  CONVERTER_FUNC(senscord_camera_calibration_property_t,
                 CameraCalibrationProperty);

  // InterlaceProperty
  CONVERTER_FUNC(senscord_interlace_property_t, InterlaceProperty);

  // InterlaceInfoProperty
  CONVERTER_FUNC(senscord_interlace_info_property_t, InterlaceInfoProperty);

  // BaselineLengthProperty
  CONVERTER_FUNC(senscord_base_line_length_property_t, BaselineLengthProperty);

  // ImuDataUnitProperty
  CONVERTER_FUNC(senscord_imu_data_unit_property_t, ImuDataUnitProperty);

  // SamplingFrequencyProperty
  // AccelerometerRangeProperty
  // GyrometerRangeProperty
  // MagnetometerRangeProperty
  //   => [Common] Scalar<float>

  // MagnetometerRange3Property
  //   => [Common] Vector3<float>

  // AccelerationCalibProperty
  // AngularVelocityCalibProperty
  // MagneticFieldCalibProperty
  //   => [Common] AxisMisalignment

  // MagneticNorthCalibProperty
  CONVERTER_FUNC(senscord_magnetic_north_calib_property_t,
                 MagneticNorthCalibProperty);

  // SlamDataSupportedProperty
  CONVERTER_FUNC(senscord_slam_data_supported_property_t,
                 SlamDataSupportedProperty);

  // InitialPoseProperty
  //   => PoseDataConverter

  // PoseDataProperty
  CONVERTER_FUNC(senscord_pose_data_property_t, PoseDataProperty);

  // OdometryDataProperty
  CONVERTER_FUNC(senscord_odometry_data_property_t, OdometryDataProperty);

  // GridSizeProperty
  CONVERTER_FUNC(senscord_grid_size_property_t, GridSizeProperty);

  // GridMapProperty
  CONVERTER_FUNC(senscord_grid_map_property_t, GridMapProperty);

  // PointCloudProperty
  CONVERTER_FUNC(senscord_point_cloud_property_t, PointCloudProperty);

  // RegisterAccess64Property
  CONVERTER_FUNC(senscord_register_access_64_property_t,
                 RegisterAccess64Property);

  // RegisterAccess32Property
  CONVERTER_FUNC(senscord_register_access_32_property_t,
                 RegisterAccess32Property);

  // RegisterAccess16Property
  CONVERTER_FUNC(senscord_register_access_16_property_t,
                 RegisterAccess16Property);

  // RegisterAccess8Property
  CONVERTER_FUNC(senscord_register_access_8_property_t,
                 RegisterAccess8Property);

  // TemperatureProperty
  CONVERTER_FUNC(senscord_temperature_property_t, TemperatureProperty);

  // PolarizationDopCorrectionProperty
  CONVERTER_FUNC(senscord_polarization_dop_correction_property_t,
                 PolarizationDopCorrectionProperty);

  // PolarizationInvalidMaskProperty
  CONVERTER_FUNC(senscord_polarization_invalid_mask_property_t,
                 PolarizationInvalidMaskProperty);

  // PolarizationNormalVectorProperty
  CONVERTER_FUNC(senscord_polarization_normal_vector_property_t,
                 PolarizationNormalVectorProperty);

  // PolarizationReflectionProperty
  CONVERTER_FUNC(senscord_polarization_reflection_property_t,
                 PolarizationReflectionProperty);

  // TemporalContrastDataProperty (PixelPolarityDataProperty)
  CONVERTER_FUNC(senscord_temporal_contrast_data_property_t,
                 TemporalContrastDataProperty);

  // RoiProperty
  CONVERTER_FUNC(senscord_roi_property_t, RoiProperty);

  // ScoreThresholdProperty
  CONVERTER_FUNC(senscord_score_threshold_property_t,
                 ScoreThresholdProperty);

  // VelocityDataUnitProperty
  CONVERTER_FUNC(senscord_velocity_data_unit_property_t,
                 VelocityDataUnitProperty);

  // DataRateProperty
  CONVERTER_FUNC(senscord_data_rate_property_t,
                 DataRateProperty);

  // CoordinateSystemProperty
  CONVERTER_FUNC(senscord_coordinate_system_property_t,
                 CoordinateSystemProperty);

  // AudioProperty
  CONVERTER_FUNC(senscord_audio_property_t, AudioProperty);

  // AudioPcmProperty
  CONVERTER_FUNC(senscord_audio_pcm_property_t, AudioPcmProperty);

  /* RawData */
  // AccelerationData
  // AngularVelocityData
  // MagneticFieldData
  //   => [Common] Vector3<float>

  // RotationData
  CONVERTER_FUNC(senscord_rotation_data_t, RotationData);

  // PoseQuaternionData
  // PoseMatrixData
  //   => PoseDataConverter

  // ObjectDetectionData
  // Please use the `senscord_deserialize_object_detection_data` API.

  // KeyPointData
  // Please use the `senscord_deserialize_key_point_data` API.

  // ObjectTrackingData
  // Please use the `senscord_deserialize_object_tracking_data` API.
};

}  // namespace senscord

#endif  // CORE_CONVERTER_SRC_CORE_CONVERTER_H_
