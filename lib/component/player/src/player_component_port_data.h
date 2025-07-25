/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_PORT_DATA_H_
#define LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_PORT_DATA_H_

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "./player_common.h"
#include "./player_component.h"
#include "./player_send_interval_manager.h"
#include "./player_frame_file_manager.h"
#include "./player_component_types.h"
#include "./player_stream_file_manager.h"
#include "senscord/develop/component.h"
#include "senscord/develop/recorder_common.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/property_types.h"
#include "senscord/noncopyable.h"

// pre-definition
class PlayerComponent;

class PlayerComponentPortData : private senscord::util::Noncopyable {
 public:
  /**
   * @brief Constructor.
   */
  explicit PlayerComponentPortData(
      int32_t port_id, PlayerComponent* player_component,
      senscord::MemoryAllocator* allocator,
      PlayerSendIntervalManager* send_interval_manager);

  /**
   * @brief Destructor.
   */
  ~PlayerComponentPortData();

  /**
   * @brief Open port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (buffer_size) The size of raw index buffer.
   * @param[in] (args) Arguments of port starting.
   * @return Status object.
   */
  senscord::Status OpenPort(
      const std::string& port_type, int32_t port_id, const size_t buffer_size,
      const senscord::ComponentPortArgument& args);

  /**
   * @brief Close port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  senscord::Status ClosePort(const std::string& port_type, int32_t port_id);

  /**
   * @brief Start port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  senscord::Status StartPort(const std::string& port_type, int32_t port_id);

  /**
   * @brief Stop port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  senscord::Status StopPort(const std::string& port_type, int32_t port_id);

  /**
   * @brief Set the serialized property.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (property_key) Key of property.
   * @param[in] (serialized_property) Serialized property address.
   * @param[in] (serialized_size) Serialized property size.
   * @return Status object.
   */
  senscord::Status SetProperty(const std::string& port_type, int32_t port_id,
                               const std::string& property_key,
                               const void* serialized_property,
                               size_t serialized_size);

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
  senscord::Status GetProperty(const std::string& port_type, int32_t port_id,
                               const std::string& property_key,
                               const void* serialized_input_property,
                               size_t serialized_input_size,
                               void** serialized_property,
                               size_t* serialized_size);

  /**
   * @brief Release the frame pushed from the port.
   * @param[in] (frameinfo) Infomation to release frame.
   */
  void ReleasePortFrame(const senscord::FrameInfo& frameinfo);

  /**
   * @brief Get target_path from property.
   * @return target_path.
   */
  const std::string& GetTargetPath() const;

  /**
   * @brief Check if the queue size is the empty
   * @return true: empty, false: not empty
   */
  bool IsFrameQueueEmpty() const;

  /**
   * @brief Publish frames with the base on frame rate.
   */
  void SendFrameThread();

  /**
   * @brief Adjust frame queue by timestamp (Discard old frame from the queue)
   * @param[in] (sent_time) The timestamp of sync position.
   * @note Use only synchronized playback.
   */
  void AdjustFrameQueueByTimestamp(uint64_t sent_time);

  /**
   * @brief Set the playback start position.
   * @param[in] (position) Playback start position to set.
   */
  void SetPlayStartPosition(uint32_t position);

 private:
  PlayerComponent* player_component_;
  senscord::MemoryAllocator* allocator_;
  senscord::osal::OSThread* send_thread_;
  bool is_started_;
  mutable senscord::osal::OSMutex* mutex_started_;

  int32_t port_id_;
  senscord::osal::OSMutex* mutex_state_;

  senscord::PlayProperty play_setting_;
  uint64_t sequence_number_;

  PlayerSendIntervalManager* send_interval_manager_;
  PlayerFrameFileManager* frame_file_manager_;
  PlayerStreamFileManager* stream_file_manager_;

  senscord::FrameRateProperty framerate_;
  senscord::ChannelInfoProperty channel_info_;

  senscord::osal::OSMutex* mutex_position_;
  uint32_t latest_position_;

  size_t composite_buffer_size_;

  /**
   * @brief Set thread started flag.
   * @param[in] (is_started) The flag of thread started
   */
  void SetThreadStarted(bool is_started);

  /**
   * @brief Start a thread
   * @return Status object
   */
  senscord::Status StartThreading();

  /**
   * @brief Stop a thread
   */
  void StopThreading();

  /**
   * @brief Check the state of the thread
   * @return true: started, false: not started
   */
  bool IsThreadStarted() const;

  /**
   * @brief Setup playdata.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (play_property) The play property.
   * @param[in] (buffer_size) The size of raw index buffer.
   * @return Status object.
   */
  senscord::Status SetupPlayManager(
      const std::string& port_type, int32_t port_id,
      const senscord::PlayProperty& play_property);

  /**
   * @brief Setup stream file manager
   * @param[in] (target_path) The path of the playback file
   * @param[in] (manager) stream file manager.
   * @return Status object.
   */
  senscord::Status SetupStreamFileManager(
      const std::string& target_path, PlayerStreamFileManager* manager);

  /**
   * @brief Setup frame file manager
   * @param[in] (play_property) The play property
   * @param[in] (channels) The channel information of info.xml
   * @param[in] (buffer_size) The size of raw index buffer.
   * @param[in] (manager) The manager of frame file
   * @return Status object.
   */
  senscord::Status SetupFrameFileManager(
      const senscord::PlayProperty& target_path,
      const InfoXmlChannelList& channels, const size_t buffer_size,
      PlayerFrameFileManager* manager);

  /**
   * @brief Clear playdata.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  void ClearPlayData(const std::string& port_type, int32_t port_id);

  /**
   * @brief Publish frames with the base on frame rate.
   */
  senscord::Status SendFrame(int32_t port_id, PlayFrame* frame);

  /**
   * @brief Register properties to player component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port.
   * @return Status object.
   */
  senscord::Status RegisterPlayProperties(
      const std::string& port_type, int32_t port_id);

  /**
   * @brief Check same path of other port.
   * @param[in] (port_id) port id.
   * @param[in] (specified_path) specified target_path.
   * @return Status object.
   */
  senscord::Status CheckSamePathOfOtherPort(
      const int32_t port_id, const std::string& specified_path);

  /**
   * @brief Check if the target path is specified
   * @return true: specified, false: not specified
   */
  bool IsSpecifiedTargetPath() const;
};

#endif  // LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_PORT_DATA_H_
