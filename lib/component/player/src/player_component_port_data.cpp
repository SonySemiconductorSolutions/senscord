/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "player_component_port_data.h"

#include <inttypes.h>
#include <stdint.h>
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "senscord/logger.h"
#include "./player_component_util.h"
#include "./player_property_accessor.h"
#include "./player_autolock.h"

namespace {
  const char* kModuleName = "player_component_port_data";

  const uint32_t kDefaultFrameRateNum = 60;
  const uint32_t kDefaultFrameRateDenom = 1;
}

/**
 * @brief Worker thread entry
 * @param[in] (arg) Arguments of port starting.
 * @return Status object.
 */
static senscord::osal::OSThreadResult WorkerThreadEntry(void* arg) {
  PlayerComponentPortData* adapter =
      reinterpret_cast<PlayerComponentPortData*>(arg);
  adapter->SendFrameThread();
  return 0;
}

/**
 * @brief Constructor.
 */
PlayerComponentPortData::PlayerComponentPortData(
    int32_t port_id, PlayerComponent* player_component,
    senscord::MemoryAllocator* allocator,
    PlayerSendIntervalManager* send_interval_manager)
    : player_component_(player_component),
      allocator_(allocator),
      send_thread_(NULL),
      is_started_(false),
      mutex_started_(NULL),
      port_id_(port_id),
      mutex_state_(NULL),
      play_setting_(),
      sequence_number_(0),
      send_interval_manager_(send_interval_manager),
      frame_file_manager_(NULL),
      stream_file_manager_(NULL),
      framerate_(),
      channel_info_(),
      latest_position_(),
      composite_buffer_size_(0) {
  senscord::osal::OSCreateMutex(&mutex_state_);
  senscord::osal::OSCreateMutex(&mutex_started_);
  senscord::osal::OSCreateMutex(&mutex_position_);
  player::ClearPlayProperty(&play_setting_);

  // Use only if the rate was obtained before the record file was specified.
  framerate_.num = kDefaultFrameRateNum;
  framerate_.denom = kDefaultFrameRateDenom;
}

/**
 * @brief Destructor.
 */
PlayerComponentPortData::~PlayerComponentPortData() {
  senscord::osal::OSDestroyMutex(mutex_position_);
  mutex_position_ = NULL;

  senscord::osal::OSDestroyMutex(mutex_state_);
  mutex_state_ = NULL;

  senscord::osal::OSDestroyMutex(mutex_started_);
  mutex_started_ = NULL;
}

/**
 * @brief Clear playdata.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
void PlayerComponentPortData::ClearPlayData(const std::string& port_type,
                                            int32_t port_id) {
  channel_info_.channels.clear();
  if (frame_file_manager_ != NULL) {
    frame_file_manager_->ClearRawIndex();
    frame_file_manager_->ClearChannel();
  }
  if (stream_file_manager_ != NULL) {
    stream_file_manager_->ClearStreamProperty();
  }
}

/**
 * @brief Open port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (buffer_size) The size of raw index buffer.
 * @param[in] (args) Arguments of port starting.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::OpenPort(
    const std::string& port_type, int32_t port_id, const size_t buffer_size,
    const senscord::ComponentPortArgument& args) {
  composite_buffer_size_ = buffer_size;
  player::AutoLock autolock(mutex_state_);
  // Check if the started stream already exists
  // - This confirmation can result in an error only synchronized playback.
  if (send_interval_manager_->GetSendStartedCount() != 0) {
    return SENSCORD_STATUS_FAIL(
      kModuleName, senscord::Status::kCauseInvalidOperation,
      "Already started of other port");
  }

  senscord::Status status = RegisterPlayProperties(port_type, port_id);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  senscord::PlayProperty play_property = {};
  status = player::OpenPortParseArg(
      port_type, port_id, args, &play_property);
  if (!status.ok()) {
    player_component_->UnregisterProperties(port_type, port_id);
    return SENSCORD_STATUS_TRACE(status);
  }

  if (!play_property.target_path.empty()) {
    // Check if the specified path already exists on another port.
    status = CheckSamePathOfOtherPort(
        port_id, play_property.target_path);
    if (!status.ok()) {
      player_component_->UnregisterProperties(port_type, port_id);
      return SENSCORD_STATUS_TRACE(status);
    }

    status = SetupPlayManager(port_type, port_id, play_property);
    if (!status.ok()) {
      player_component_->UnregisterProperties(port_type, port_id);
      return SENSCORD_STATUS_TRACE(status);
    }
  } else {
    // regards this case as OK
    // Because, if the target_path is not specified in senscord.xml,
    // the setup process is performed when the PlayProperty is set.
  }

  return senscord::Status::OK();
}

/**
 * @brief Close port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::ClosePort(
    const std::string& port_type, int32_t port_id) {
  player::AutoLock autolock(mutex_state_);

  player_component_->UnregisterProperties(port_type, port_id);
  ClearPlayData(port_type, port_id);
  player::ClearPlayProperty(&play_setting_);

  send_interval_manager_->FinalizeSendIntervalManager(port_id);

  delete stream_file_manager_;
  stream_file_manager_ = NULL;
  delete frame_file_manager_;
  frame_file_manager_ = NULL;

  return senscord::Status::OK();
}

/**
 * @brief Start port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::StartPort(
    const std::string& port_type, int32_t port_id) {
  if (!IsSpecifiedTargetPath()) {
    return SENSCORD_STATUS_FAIL(
      kModuleName, senscord::Status::kCauseInvalidOperation,
      "Not specify target file");
  }
  senscord::Status status;

  // Start SendFrameThread
  SetThreadStarted(true);
  status = StartThreading();
  SENSCORD_STATUS_TRACE(status);

  // Start ReadFrameThread
  if (status.ok()) {
    frame_file_manager_->SetThreadStarted(true);
    status = frame_file_manager_->StartThreading();
    SENSCORD_STATUS_TRACE(status);
  }

  // Start SendIntervalThread
  if (status.ok()) {
    send_interval_manager_->SetThreadStarted(port_id, true);
    status = send_interval_manager_->StartThreading(port_id);
    SENSCORD_STATUS_TRACE(status);
  }

  if (!status.ok()) {
    // roll-back
    StopPort(port_type, port_id);
  }

  return status;
}

/**
 * @brief Stop port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::StopPort(const std::string& port_type,
                                                   int32_t port_id) {
  senscord::Status status;

  send_interval_manager_->SetThreadStarted(port_id, false);
  frame_file_manager_->SetThreadStarted(false);
  SetThreadStarted(false);

  frame_file_manager_->StopThreading();
  send_interval_manager_->StopThreading(port_id);
  StopThreading();

  // playback position after stop.
  SetPlayStartPosition(play_setting_.start_offset);

  return senscord::Status::OK();
}

/**
 * @brief Set the serialized property.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (property_key) Key of property.
 * @param[in] (serialized_property) Serialized property address.
 * @param[in] (serialized_size) Serialized property size.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::SetProperty(
    const std::string& port_type, int32_t port_id,
    const std::string& property_key,
    const void* serialized_property, size_t serialized_size) {
  player::AutoLock autolock(mutex_state_);
  senscord::Status status;

  std::string key = senscord::PropertyUtils::GetKey(property_key);
  if (key == senscord::kPlayPropertyKey) {
    senscord::serialize::Decoder decoder(serialized_property, serialized_size);
    senscord::PlayProperty play_property = {};
    status = decoder.Pop(play_property);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      if (IsThreadStarted()) {
          status = SENSCORD_STATUS_FAIL(
            kModuleName, senscord::Status::kCauseInvalidOperation,
            "Already started");
      } else if (send_interval_manager_->GetSendStartedCount() > 0) {
        return SENSCORD_STATUS_FAIL(
          kModuleName, senscord::Status::kCauseInvalidOperation,
          "Already started of other port");
      }
    }

    if (status.ok()) {
      // Check if the specified path already exists on another port.
      status = CheckSamePathOfOtherPort(port_id, play_property.target_path);
      SENSCORD_STATUS_TRACE(status);
    }

    if (status.ok()) {
      // Replace the play speed with frame rate.
      play_property.speed = senscord::kPlaySpeedBasedOnFramerate;

      // Setup manager
      status = SetupPlayManager(port_type, port_id, play_property);
      SENSCORD_STATUS_TRACE(status);
    }
  } else if (key == senscord::kPlayPositionPropertyKey) {
    senscord::serialize::Decoder decoder(serialized_property, serialized_size);
    senscord::PlayPositionProperty prop = {};
    status = decoder.Pop(prop);
    if (status.ok()) {
      if (frame_file_manager_ == NULL) {
        // playback file not specified
        return SENSCORD_STATUS_FAIL(
            kModuleName, senscord::Status::kCauseInvalidOperation,
            "Incomplete playback parameters.");
      } else if (send_interval_manager_->GetSendManagePortCount() > 1) {
        // synchronous playback
        return SENSCORD_STATUS_FAIL(
            kModuleName, senscord::Status::kCauseNotSupported,
            "Not supported synchronous playback");
      } else if (IsThreadStarted()) {
        return SENSCORD_STATUS_FAIL(
            kModuleName, senscord::Status::kCauseInvalidOperation,
            "Already started");
      }
      SetPlayStartPosition(prop.position);
    }
  } else if (key == senscord::kPlayModePropertyKey) {
    senscord::serialize::Decoder decoder(serialized_property, serialized_size);
    senscord::PlayModeProperty prop = {};
    status = decoder.Pop(prop);
    if (status.ok()) {
      send_interval_manager_->SetRepeatMode(prop.repeat);
      play_setting_.mode = prop;
    }
    return SENSCORD_STATUS_TRACE(status);
  } else if (key == senscord::kPlayFileInfoPropertyKey) {
    status = SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseNotSupported,
        "SetProperty(key='%s') is not supported.", key.c_str());
  } else if (key == senscord::kFrameRatePropertyKey) {
    senscord::serialize::Decoder decoder(serialized_property, serialized_size);
    senscord::FrameRateProperty property = {};
    status = decoder.Pop(property);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      if (property.num == 0 || property.denom == 0) {
        status = SENSCORD_STATUS_FAIL(kModuleName,
          senscord::Status::kCauseInvalidOperation,
          "Invalid framerate: num=%" PRIu32 ", denom=%" PRIu32,
          property.num, property.denom);
      }
    }

    if (status.ok()) {
      // apply frame rate
      framerate_.num = property.num;
      framerate_.denom = property.denom;

      // apply frame rate to manager
      if (IsSpecifiedTargetPath()) {
        frame_file_manager_->SetReadSleepTime(property.num, property.denom);
        send_interval_manager_->SetFrameRate(
            port_id, property.num, property.denom);
      }
    }
  } else {
    status = stream_file_manager_->SetStreamProperty(
        key, serialized_property, serialized_size);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Get and create new serialized property.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (property_key) Key of property.
 * @param[in] (serialized_input_property) Input serialized property address.
 * @param[in] (serialized_input_size) Input serialized property size.
 * @param[out] (serialized_property) New serialized property address.
 * @param[out] (serialized_size) Serialized property size.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::GetProperty(
    const std::string& port_type, int32_t port_id,
    const std::string& property_key,
    const void* serialized_input_property, size_t serialized_input_size,
    void** serialized_property, size_t* serialized_size) {
  player::AutoLock autolock(mutex_state_);
  senscord::Status status;

  std::string key = senscord::PropertyUtils::GetKey(property_key);
  if (key == senscord::kPlayPropertyKey) {
    senscord::PlayProperty prop = play_setting_;

    if (IsSpecifiedTargetPath()) {
      // Get the count of playback frames
      prop.count = frame_file_manager_->GetPlayCount();
    }

    senscord::serialize::SerializedBuffer buffer;
    senscord::serialize::Encoder encoder(&buffer);
    status = encoder.Push(prop);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      *serialized_size = buffer.size();
      *serialized_property = new uint8_t[*serialized_size]();
      senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                               buffer.data(), buffer.size());
    }
  } else if (key == senscord::kPlayModePropertyKey) {
    senscord::PlayModeProperty prop = play_setting_.mode;
    senscord::serialize::SerializedBuffer buffer;
    senscord::serialize::Encoder encoder(&buffer);
    status = encoder.Push(prop);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      *serialized_size = buffer.size();
      *serialized_property = new uint8_t[*serialized_size]();
      senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                               buffer.data(), buffer.size());
    }
  } else if (key == senscord::kPlayFileInfoPropertyKey) {
    if (stream_file_manager_ != NULL && frame_file_manager_ != NULL) {
      senscord::PlayFileInfoProperty prop;
      prop.target_path = play_setting_.target_path;
      stream_file_manager_->GetPlayFileInfo(&prop);
      prop.frame_count = frame_file_manager_->GetTotalFrameCount();
      senscord::serialize::SerializedBuffer buffer;
      senscord::serialize::Encoder encoder(&buffer);
      status = encoder.Push(prop);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok()) {
        *serialized_size = buffer.size();
        *serialized_property = new uint8_t[*serialized_size]();
        senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                                 buffer.data(), buffer.size());
      }
    } else {
      status = SENSCORD_STATUS_FAIL(kModuleName,
          senscord::Status::kCauseInvalidOperation,
          "Incomplete playback parameters.");
    }
  } else if (key == senscord::kPlayPositionPropertyKey) {
    if (frame_file_manager_ != NULL &&
        frame_file_manager_->GetPlayCount() != 0) {
      senscord::PlayPositionProperty prop = {};
      {
        player::AutoLock position_lock(mutex_position_);
        prop.position = latest_position_;
      }
      senscord::serialize::SerializedBuffer buffer;
      senscord::serialize::Encoder encoder(&buffer);
      status = encoder.Push(prop);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok()) {
        *serialized_size = buffer.size();
        *serialized_property = new uint8_t[*serialized_size]();
        senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                                 buffer.data(), buffer.size());
      }
    } else {
      status = SENSCORD_STATUS_FAIL(
          kModuleName, senscord::Status::kCauseInvalidOperation,
          "Incomplete playback parameters.");
    }
  } else if (key == senscord::kFrameRatePropertyKey) {
    senscord::serialize::SerializedBuffer buffer;
    senscord::serialize::Encoder encoder(&buffer);
    status = encoder.Push(framerate_);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      *serialized_size = buffer.size();
      *serialized_property = new uint8_t[*serialized_size]();
      senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                               buffer.data(), buffer.size());
    }
  } else if (key == senscord::kChannelInfoPropertyKey) {
    senscord::serialize::SerializedBuffer buffer;
    senscord::serialize::Encoder encoder(&buffer);
    status = encoder.Push(channel_info_);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      *serialized_size = buffer.size();
      *serialized_property = new uint8_t[*serialized_size]();
      senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                               buffer.data(), buffer.size());
    }
  } else {
    const std::vector<uint8_t>* property_data =
        stream_file_manager_->GetStreamProperty(key);
    if (property_data == NULL) {
      status = SENSCORD_STATUS_FAIL(kModuleName,
          senscord::Status::kCauseInvalidOperation,
          "failed to find key=%s", key.c_str());
    }

    if (status.ok()) {
      *serialized_size = property_data->size();
      *serialized_property = new uint8_t[*serialized_size]();

      senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
                                property_data->data(), property_data->size());
    }
  }
  return status;
}

/**
 * @brief Set thread started flag.
 * @param[in] (is_started) The flag of thread started
 */
void PlayerComponentPortData::SetThreadStarted(bool is_started) {
  player::AutoLock autolock(mutex_started_);
  is_started_ = is_started;
}

/**
 * @brief Check the state of the thread
 * @return true: started, false: not started
 */
bool PlayerComponentPortData::IsThreadStarted() const {
  player::AutoLock autolock(mutex_started_);
  return is_started_;
}

/**
 * @brief Start a thread
 * @return Status object
 */
senscord::Status PlayerComponentPortData::StartThreading() {
  int32_t ret = senscord::osal::OSCreateThread(
      &send_thread_, WorkerThreadEntry, this, NULL);
  if (ret != 0) {
    return SENSCORD_STATUS_FAIL(kModuleName,
        senscord::Status::kCauseAborted,
        "failed to OSCreateThread: %" PRIx32, ret);
  }
  return senscord::Status::OK();
}

/**
 * @brief Stop a thread
 */
void PlayerComponentPortData::StopThreading() {
  // wait for thread stop
  senscord::osal::OSJoinThread(send_thread_, NULL);
}

/**
 * @brief Publish frames with the base on frame rate.
 */
void PlayerComponentPortData::SendFrameThread() {
  // Send loop.
  senscord::Status status;
  send_interval_manager_->SetFrameWait(port_id_, true);
  while (1) {
    status = send_interval_manager_->WaitInterval(port_id_);
    if (!IsThreadStarted()) {
      break;
    }
    if (!status.ok()) {
      SENSCORD_LOG_DEBUG("WaitInterval(): %s", status.ToString().c_str());
      continue;
    }

    PlayFrame* frame = NULL;
    status = frame_file_manager_->GetFrame(&frame);
    if (!status.ok()) {
      SENSCORD_LOG_WARNING("Failed to get the frame : ret=%s",
        status.ToString().c_str());
    } else {
      status = SendFrame(port_id_, frame);
      if (!status.ok()) {  // NG SendFrame()
        SENSCORD_LOG_WARNING("Failed to send the frame : ret=%s",
          status.ToString().c_str());
      }
    }

    delete frame;
  }  // while (1)

  send_interval_manager_->SetFrameWait(port_id_, false);
}

/**
 * @brief Release the frame pushed from the port.
 * @param[in] (frameinfo) Infomation to release frame.
 */
void PlayerComponentPortData::ReleasePortFrame(
    const senscord::FrameInfo& frameinfo) {
  std::vector<senscord::ChannelRawData>::const_iterator itr =
      frameinfo.channels.begin();
  std::vector<senscord::ChannelRawData>::const_iterator end =
      frameinfo.channels.end();
  for (; itr != end; ++itr) {
    if (itr->data_memory != NULL) {
      allocator_->Free(itr->data_memory);
    }
  }
}

/**
 * @brief Register properties to player component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::RegisterPlayProperties(
    const std::string& port_type, int32_t port_id) {
  senscord::Status status;
  PropertyKeyList key_list;
  // register key list
  key_list.push_back(senscord::kPlayPropertyKey);
  key_list.push_back(senscord::kPlayModePropertyKey);
  key_list.push_back(senscord::kPlayFileInfoPropertyKey);
  key_list.push_back(senscord::kPlayPositionPropertyKey);
  key_list.push_back(senscord::kFrameRatePropertyKey);
  key_list.push_back(senscord::kChannelInfoPropertyKey);
  status = player_component_->RegisterProperties(port_type, port_id, key_list);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Setup play manager
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (play_property) The play property.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::SetupPlayManager(
    const std::string& port_type, int32_t port_id,
    const senscord::PlayProperty& play_property) {
  senscord::Status status;
  // Setup stream file manager
  PlayerStreamFileManager* stream_file_manager = new PlayerStreamFileManager();
  status = SetupStreamFileManager(
      play_property.target_path, stream_file_manager);
  if (!status.ok()) {
    delete stream_file_manager;
    return SENSCORD_STATUS_TRACE(status);
  }

  InfoXmlChannelList channels;
  stream_file_manager->GetInfoXmlChannels(&channels);

  senscord::FrameRateProperty frame_rate_property = {};
  stream_file_manager->GetFrameRate(&frame_rate_property);

  // Setup frame file manager
  PlayerFrameFileManager* frame_file_manager =
      new PlayerFrameFileManager(allocator_);
  status = SetupFrameFileManager(
      play_property, channels, composite_buffer_size_, frame_file_manager);
  if (!status.ok()) {
    delete stream_file_manager;
    delete frame_file_manager;
    return SENSCORD_STATUS_TRACE(status);
  }

  frame_file_manager->GetChannelInfo(&channel_info_);
  player_component_->SetType(
      port_type, port_id, stream_file_manager->GetStreamType());

  // Update StreamProperty
  if (stream_file_manager_ != NULL) {
    // Removing old property
    std::vector<std::string> prev_property_key_list;
    status =
        stream_file_manager_->GetStreamPropertyList(&prev_property_key_list);
    if (status.ok()) {
      status = player_component_->DeleteProperties(
          port_type, port_id, prev_property_key_list);
    }
    if (!status.ok()) {
      delete stream_file_manager;
      delete frame_file_manager;
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  {
    // Add a new property
    std::vector<std::string> curr_property_key_list;
    status =
      stream_file_manager->GetStreamPropertyList(&curr_property_key_list);
    if (status.ok()) {
      status = player_component_->AddProperties(
          port_type, port_id, curr_property_key_list);
    }
    if (!status.ok()) {
      delete stream_file_manager;
      delete frame_file_manager;
      return SENSCORD_STATUS_TRACE(status);
    }
  }

  // delete old instance
  if (stream_file_manager_ != NULL) {
    delete stream_file_manager_;
  }
  if (frame_file_manager_ != NULL) {
    delete frame_file_manager_;
  }

  // apply
  bool is_diff_file_path =
      play_setting_.target_path != play_property.target_path;
  stream_file_manager_ = stream_file_manager;
  frame_file_manager_ = frame_file_manager;
  play_setting_ = play_property;
  framerate_ = frame_rate_property;
  send_interval_manager_->SetupSendIntervalManager(
      port_id_, frame_file_manager_->GetSentTimeList(), this);
  send_interval_manager_->SetFrameRate(
      port_id, framerate_.num, framerate_.denom);
  send_interval_manager_->SetRepeatMode(play_property.mode.repeat);
  frame_file_manager_->SetReadSleepTime(framerate_.num, framerate_.denom);
  if (is_diff_file_path) {
    // reset start position
    SetPlayStartPosition(play_setting_.start_offset);
  } else {
    // corrects the current playback position to within the playback range
    SetPlayStartPosition(latest_position_);
  }

  return senscord::Status::OK();
}

/**
 * @brief Setup stream file manager
 * @param[in] (target_path) The path of the playback file
 * @param[in] (manager) stream file manager.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::SetupStreamFileManager(
  const std::string& target_path, PlayerStreamFileManager* manager) {
  senscord::Status status = manager->ReadStreamFile(target_path);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  return senscord::Status::OK();
}

/**
 * @brief Setup frame file manager
 * @param[in] (play_property) The play property
 * @param[in] (channels) The channel information of info.xml
 * @param[in] (buffer_size) The size of raw index buffer.
 * @param[in] (manager) The manager of frame file
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::SetupFrameFileManager(
    const senscord::PlayProperty& play_property,
    const InfoXmlChannelList& channels, const size_t buffer_size,
    PlayerFrameFileManager* manager) {
  senscord::Status status = manager->SetupFrameFileManager(
      play_property.target_path, channels, buffer_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  status = manager->SetPlaybackRange(
      play_property.start_offset, play_property.count);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  return senscord::Status::OK();
}

/**
 * @brief Check same path of other port.
 * @param[in] (port_id) port id.
 * @param[in] (specified_path) specified target_path.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::CheckSamePathOfOtherPort(
    const int32_t port_id, const std::string& specified_path) {
  std::map<int32_t, std::string> paths;
  senscord::Status status = player_component_->GetTargetPathList(&paths);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  std::map<int32_t, std::string>::const_iterator itr = paths.begin();
  std::map<int32_t, std::string>::const_iterator end = paths.end();
  for (; itr != end; ++itr) {
    if (itr->first == port_id) {
      continue;
    }
    if (itr->second == specified_path) {
      return SENSCORD_STATUS_FAIL(kModuleName,
          senscord::Status::kCauseInvalidOperation,
          "specified target_path that already exists: %s",
          specified_path.c_str());
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Get target_path from property.
 * @return target_path.
 */
const std::string& PlayerComponentPortData::GetTargetPath() const {
  return play_setting_.target_path;
}

/**
 * @brief Check if the target path is specified
 * @return true: specified, false: not specified
 */
bool PlayerComponentPortData::IsSpecifiedTargetPath() const {
  bool is_specified = true;
  if (play_setting_.target_path.empty()) {
    is_specified = false;
  }
  return is_specified;
}

/**
 * @brief Send the frame to the connected stream.
 * @param[in] (player_component) Player component.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frame_data_list) The list of frame data to send.
 * @return Status object.
 */
senscord::Status PlayerComponentPortData::SendFrame(
    int32_t port_id, PlayFrame* frame) {
  senscord::Status status;
  // update channel property
  std::map<uint32_t, BinaryPropertyList>::iterator channel_itr =
      frame->properties.begin();
  for (; channel_itr != frame->properties.end(); ++channel_itr) {
    const uint32_t& channel_id = channel_itr->first;
    BinaryPropertyList::const_iterator property_itr =
        channel_itr->second.begin();
    for (; property_itr != channel_itr->second.end(); ++property_itr) {
      status = player_component_->UpdateFrameProperty(
          port_id, channel_id, property_itr->first, &(property_itr->second));
      if (!status.ok()) {
        SENSCORD_LOG_WARNING(
            "[%s] UpdateFrameProperty NG(%s).",
            kModuleName, status.ToString().c_str());
        // Continue sending even if the property update fails
      }
    }
  }

  // update playback position.
  uint32_t position = 0;
  {
    player::AutoLock autolock(mutex_position_);
    position = latest_position_ = play_setting_.start_offset + frame->index;
  }
  for (std::vector<senscord::ChannelRawData>::const_iterator
      itr = frame->frame_info.channels.begin(),
      end = frame->frame_info.channels.end(); itr != end; ++itr) {
    player_component_->UpdatePlayPositionProperty(
        port_id, itr->channel_id, position);
  }

  // send frame
  frame->frame_info.sequence_number = sequence_number_++;
  status = player_component_->SendFrame(port_id, frame->frame_info);
  if (!status.ok()) {
    SENSCORD_LOG_WARNING("[%s] SendFrame NG(% " PRIu64 ") : ret=%s",
        kModuleName, frame->frame_info.sequence_number,
        status.ToString().c_str());
    ReleasePortFrame(frame->frame_info);
    return SENSCORD_STATUS_TRACE(status);
  }

  return senscord::Status::OK();
}

/**
 * @brief Check if the queue size is the empty
 * @return true: empty, false: not empty
 */
bool PlayerComponentPortData::IsFrameQueueEmpty() const {
  return frame_file_manager_->IsFrameQueueEmpty();
}

/**
 * @brief Adjust frame queue by timestamp (Discard old frame from the queue)
 * @param[in] (sent_time) The timestamp of sync position.
 * @note Use only synchronized playback.
 */
void PlayerComponentPortData::AdjustFrameQueueByTimestamp(uint64_t sent_time) {
  frame_file_manager_->AdjustFrameQueueByTimestamp(sent_time);
}

/**
 * @brief Set the playback start position.
 * @param[in] (position) Playback start position to set.
 */
void PlayerComponentPortData::SetPlayStartPosition(uint32_t position) {
  player::AutoLock autolock(mutex_state_);
  if (frame_file_manager_ != NULL) {
    uint32_t count = frame_file_manager_->GetPlayCount();
    // correct position
    position = std::max(position, play_setting_.start_offset);
    position = std::min(
        position, (play_setting_.start_offset + count - 1));
    SENSCORD_LOG_DEBUG(
        "[%" PRIu32 "] position:%" PRIu32 " (offset:%" PRIu32
        ", count:%" PRIu32 ")",
        port_id_, position, play_setting_.start_offset, count);
    // set position
    {
      player::AutoLock position_lock(mutex_position_);
      latest_position_ = position;
      frame_file_manager_->SetPlayStartPosition(
          latest_position_ - play_setting_.start_offset);
      send_interval_manager_->SetAsyncPlayStartPosition(
          latest_position_ - play_setting_.start_offset);
    }
  }
}
