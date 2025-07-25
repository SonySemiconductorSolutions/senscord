/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senscord/config.h"

#ifdef SENSCORD_SERIALIZE

#include <inttypes.h>
#include <vector>

#include "senscord/c_api/senscord_c_api.h"
#include "senscord/serialize.h"
#include "senscord/rawdata_types.h"
#include "senscord/osal_inttypes.h"
#include "c_api/c_common.h"

namespace c_api = senscord::c_api;

namespace {
void convert_to_vector2f(const senscord::Vector2<float>& in_value,
                         senscord_vector2f_t* out_value) {
  out_value->x = in_value.x;
  out_value->y = in_value.y;
}

void convert_to_vector2u32(const senscord::Vector2<uint32_t>& in_value,
                         senscord_vector2u32_t* out_value) {
  out_value->x = in_value.x;
  out_value->y = in_value.y;
}

void convert_to_vector3f(const senscord::Vector3<float>& in_value,
                         senscord_vector3f_t* out_value) {
  out_value->x = in_value.x;
  out_value->y = in_value.y;
  out_value->z = in_value.z;
}

void convert_to_quaternion_f(const senscord::Quaternion<float>& in_value,
                             senscord_quaternion_f_t* out_value) {
  out_value->x = in_value.x;
  out_value->y = in_value.y;
  out_value->z = in_value.z;
  out_value->w = in_value.w;
}

void convert_to_senscord_matrix3x3f_t(
    const senscord::Matrix3x3<float>& in_value,
    senscord_matrix3x3f_t* out_value) {
  out_value->element[0][0] = in_value.element[0][0];
  out_value->element[0][1] = in_value.element[0][1];
  out_value->element[0][2] = in_value.element[0][2];
  out_value->element[1][0] = in_value.element[1][0];
  out_value->element[1][1] = in_value.element[1][1];
  out_value->element[1][2] = in_value.element[1][2];
  out_value->element[2][0] = in_value.element[2][0];
  out_value->element[2][1] = in_value.element[2][1];
  out_value->element[2][2] = in_value.element[2][2];
}

void convert_to_rectangle_region_parameter(
  const senscord::RectangleRegionParameter& in_value,
  senscord_rectangle_region_parameter_t* out_value) {
    out_value->top    = in_value.top;
    out_value->left   = in_value.left;
    out_value->bottom = in_value.bottom;
    out_value->right  = in_value.right;
}

void convert_to_pose_quaternion(const senscord::PoseQuaternionData& in_value,
                     senscord_pose_quaternion_data_t* out_value) {
  convert_to_vector3f(in_value.position, &out_value->position);
  convert_to_quaternion_f(in_value.orientation, &out_value->orientation);
}

void convert_to_pose_matrix(const senscord::PoseMatrixData& in_value,
                     senscord_pose_matrix_data_t* out_value) {
  convert_to_vector3f(in_value.position, &out_value->position);
  convert_to_senscord_matrix3x3f_t(in_value.rotation, &out_value->rotation);
}

void convert_to_object_detection_data(
    const senscord::DetectedObjectInformation& in_value,
                     senscord_detected_object_information_t* out_value) {
  out_value->class_id = in_value.class_id;
  out_value->score    = in_value.score;
  convert_to_rectangle_region_parameter(in_value.box, &out_value->box);
}

void convert_to_key_point_info(
    const senscord::DetectedKeyPointInformation& in_value,
                     senscord_detected_key_point_information_t* out_value) {
  out_value->class_id = in_value.class_id;
  out_value->score    = in_value.score;
}

void convert_to_key_point(const senscord::KeyPoint& in_value,
                     senscord_key_point_t* out_value) {
  out_value->key_point_id = in_value.key_point_id;
  out_value->score        = in_value.score;
  convert_to_vector3f(in_value.point, &out_value->point);
}

void convert_to_object_tracking_data(
    const senscord::TrackedObjectInformation& in_value,
                     senscord_tracked_object_information_t* out_value) {
  out_value->track_id = in_value.track_id;
  out_value->class_id = in_value.class_id;
  out_value->score    = in_value.score;
  convert_to_vector2f(in_value.velocity, &out_value->velocity);
  convert_to_vector2u32(in_value.position, &out_value->position);
  convert_to_rectangle_region_parameter(in_value.box, &out_value->box);
}

}  // namespace

/**
 * @brief Deserialize raw data.
 * (AccelerationData, AngularVelocityData, MagneticFieldData)
 *
 * To release, you need to call the
 * senscord_release_vector3_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized vector3 data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_vector3_data
 */
int32_t senscord_deserialize_vector3_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_vector3f_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::Vector3<float> tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_vector3f_t* new_data = new senscord_vector3f_t;
  convert_to_vector3f(tmp, new_data);
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release Vector3 data.
 * @param[in] data  Vector3 data.
 */
void senscord_release_vector3_data(
    struct senscord_vector3f_t* data) {
  if (data == NULL) {
    return;
  }
  delete data;
}

/**
 * @brief Deserialize raw data. (RotationData)
 *
 * To release, you need to call the
 * senscord_release_rotation_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized rotation data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_rotation_data
 */
int32_t senscord_deserialize_rotation_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_rotation_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::RotationData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_rotation_data_t* new_data = new senscord_rotation_data_t;
  new_data->roll = tmp.roll;
  new_data->pitch = tmp.pitch;
  new_data->yaw = tmp.yaw;
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release rotation data.
 * @param[in] data  Rotation data.
 */
void senscord_release_rotation_data(
    struct senscord_rotation_data_t* data) {
  if (data == NULL) {
    return;
  }
  delete data;
}

/**
 * @brief Deserialize raw data. (PoseQuaternionData)
 *
 * To release, you need to call the
 * senscord_release_pose_quaternion_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized pose data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_pose_quaternion_data
 */
int32_t senscord_deserialize_pose_quaternion_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_pose_quaternion_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::PoseQuaternionData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_pose_quaternion_data_t* new_data =
      new senscord_pose_quaternion_data_t;
  convert_to_pose_quaternion(tmp, new_data);
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release Pose(quaternion) data.
 * @param[in] data  Pose data.
 */
void senscord_release_pose_quaternion_data(
    struct senscord_pose_quaternion_data_t* data) {
  if (data == NULL) {
    return;
  }
  delete data;
}

/**
 * @brief Deserialize raw data. (PoseData)
 *
 * To release, you need to call the
 * senscord_release_pose_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized pose data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_pose_data
 * @deprecated will be replaced by senscord_deserialize_pose_quaternion_data
 */
int32_t senscord_deserialize_pose_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_pose_data_t** deserialized_data) {
  return senscord_deserialize_pose_quaternion_data(
      raw_data, raw_data_size,
      reinterpret_cast<senscord_pose_quaternion_data_t**>(deserialized_data));
}

/**
 * @brief Release Pose data.
 * @param[in] data  Pose data.
 * @deprecated will be replaced by senscord_release_pose_quaternion_data
 */
void senscord_release_pose_data(
    struct senscord_pose_data_t* data) {
  senscord_release_pose_quaternion_data(
      reinterpret_cast<senscord_pose_quaternion_data_t*>(data));
}

/**
 * @brief Deserialize raw data. (PoseMatrixData)
 *
 * To release, you need to call the
 * senscord_release_pose_matrix_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized pose data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_pose_matrix_data
 */
int32_t senscord_deserialize_pose_matrix_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_pose_matrix_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::PoseMatrixData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_pose_matrix_data_t* new_data = new senscord_pose_matrix_data_t;
  convert_to_pose_matrix(tmp, new_data);
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release Pose(rotation matrix) data.
 * @param[in] data  Pose data.
 */
void senscord_release_pose_matrix_data(
    struct senscord_pose_matrix_data_t* data) {
  if (data == NULL) {
    return;
  }
  delete data;
}

/**
 * @brief Deserialize raw data. (ObjectDetectionData)
 *
 * To release, you need to call the
 * senscord_release_object_detection_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized Object Detection data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_object_detection_data
 */
int32_t senscord_deserialize_object_detection_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_object_detection_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::ObjectDetectionData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_object_detection_data_t* new_data =
                     new senscord_object_detection_data_t;
  new_data->data = NULL;
  new_data->count = static_cast<uint32_t>(tmp.data.size());
  if (new_data->count != 0) {
    new_data->data =
        new senscord_detected_object_information_t[new_data->count];
    for (uint32_t i = 0; i < new_data->count; ++i) {
      convert_to_object_detection_data(tmp.data[i], &new_data->data[i]);
    }
  }
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release ObjectDetection data.
 * @param[in] data  Object Detection data.
 */
void senscord_release_object_detection_data(
    struct senscord_object_detection_data_t* data) {
  if (data != NULL) {
    if (data->data != NULL) {
      delete[] data->data;
    }
    delete data;
  }
  return;
}

/**
 * @brief Deserialize raw data. (keyPointData)
 *
 * To release, you need to call the
 * senscord_release_key_point_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized KeyPoint data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_key_point_data
 */
int32_t senscord_deserialize_key_point_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_key_point_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::KeyPointData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_key_point_data_t* new_data =
                     new senscord_key_point_data_t;
  new_data->data  = NULL;
  new_data->count = static_cast<uint32_t>(tmp.data.size());
  if (new_data->count != 0) {
    new_data->data =
        new senscord_detected_key_point_information_t[new_data->count];
    for (uint32_t i = 0; i < new_data->count; ++i) {
      convert_to_key_point_info(tmp.data[i], &new_data->data[i]);
      new_data->data[i].key_points = NULL;
      new_data->data[i].count =
          static_cast<uint32_t>(tmp.data[i].key_points.size());
      if (new_data->data[i].count != 0) {
        new_data->data[i].key_points =
            new senscord_key_point_t[new_data->data[i].count];
        for (uint32_t j = 0; j < new_data->data[i].count; ++j) {
          convert_to_key_point(
              tmp.data[i].key_points[j], &new_data->data[i].key_points[j]);
        }
      }
    }
  }
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release KeyPoint data.
 * @param[in] data  KeyPoint data.
 */
void senscord_release_key_point_data(
    struct senscord_key_point_data_t* data) {

  if (data != NULL) {
    if (data->data != NULL) {
      for (uint32_t i = 0; i < data->count; ++i) {
        if (data->data[i].key_points != NULL) {
          delete[] data->data[i].key_points;
        }
      }
      delete[] data->data;
    }
    delete data;
  }
  return;
}

/**
 * @brief Deserialize raw data. (ObjectTrackingData)
 *
 * To release, you need to call the
 * senscord_release_object_tracking_data() function.
 *
 * @param[in]  raw_data           Raw data.
 * @param[in]  raw_data_size      Raw data size.
 * @param[out] deserialized_data  Deserialized Object Tracking data.
 * @return 0 is success or minus is failed (error code).
 * @see senscord_release_object_tracking_data
 */
int32_t senscord_deserialize_object_tracking_data(
    const void* raw_data, size_t raw_data_size,
    struct senscord_object_tracking_data_t** deserialized_data) {
  if (raw_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "raw_data == NULL"));
    return -1;
  }
  if (deserialized_data == NULL) {
    c_api::SetLastError(SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "deserialized_data == NULL"));
    return -1;
  }
  senscord::ObjectTrackingData tmp = {};
  senscord::serialize::Decoder decoder(raw_data, raw_data_size);
  senscord::Status status = decoder.Pop(tmp);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord_object_tracking_data_t* new_data =
                     new senscord_object_tracking_data_t;
  new_data->data = NULL;
  new_data->count = static_cast<uint32_t>(tmp.data.size());
  if (new_data->count != 0) {
    new_data->data =
        new senscord_tracked_object_information_t[new_data->count];
    for (uint32_t i = 0; i < new_data->count; ++i) {
      convert_to_object_tracking_data(tmp.data[i], &new_data->data[i]);
    }
  }
  *deserialized_data = new_data;
  return 0;
}

/**
 * @brief Release ObjectTracking data.
 * @param[in] data  Object Tracking data.
 */
void senscord_release_object_tracking_data(
    struct senscord_object_tracking_data_t* data) {
  if (data != NULL) {
    if (data->data != NULL) {
      delete[] data->data;
    }
    delete data;
  }
  return;
}

#endif  // SENSCORD_SERIALIZE
