/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_H_
#define LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_H_

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "./player_send_interval_manager.h"
#include "./player_component_types.h"
#include "./player_property_accessor.h"
#include "senscord/develop/component.h"
#include "senscord/develop/recorder_common.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/property_types.h"

// pre-definition
class PlayerComponentPortData;

/**
 * @brief The player component.
 */
class PlayerComponent : public senscord::Component {
 public:
  /**
   * @brief Constructor.
   */
  PlayerComponent();

  /**
   * @brief Destructor.
   */
  ~PlayerComponent();

  /**
   * @brief Implemented initialization.
   */
  virtual senscord::Status InitComponent(
      senscord::ComponentPortManager* port_manager,
      const senscord::ComponentArgument& args);

  /**
   * @brief Implemented exit.
   */
  virtual senscord::Status ExitComponent();

  /**
   * @brief Implemented open the port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (args) The arg of component port.
   * @return Status object.
   */
  virtual senscord::Status OpenPort(
      const std::string& port_type, int32_t port_id,
      const senscord::ComponentPortArgument& args);

  /**
   * @brief Implemented close the port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  virtual senscord::Status ClosePort(const std::string& port_type,
                                     int32_t port_id);

  /**
   * @brief Start the port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  virtual senscord::Status StartPort(const std::string& port_type,
                                     int32_t port_id);

  /**
   * @brief Stop the port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  virtual senscord::Status StopPort(const std::string& port_type,
                                    int32_t port_id);

  /**
   * Release the frame pushed from the port.
   */
  virtual senscord::Status ReleasePortFrame(
      const std::string& port_type, int32_t port_id,
      const senscord::FrameInfo& frameinfo,
      const std::vector<uint32_t>* referenced_channel_ids);

  /**
   * @brief Release the frame pushed from the port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (frameinfo) Information to release frame.
   * @param[in] (referenced_channel_ids) List of referenced channel IDs.
   * @return Status object.
   */
  senscord::Status SetProperty(const std::string& port_type, int32_t port_id,
                               const std::string& key,
                               const void* serialized_property,
                               size_t serialized_size);

  /**
   * @brief Get and create new serialized property.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (key) Key of property.
   * @param[in] (serialized_input_property) Input serialized property address.
   * @param[in] (serialized_input_size) Input serialized property size.
   * @param[out] (serialized_property) New serialized property address.
   * @param[out] (serialized_size) Serialized property size.
   * @return Status object.
   */
  senscord::Status GetProperty(const std::string& port_type, int32_t port_id,
                               const std::string& key,
                               const void* serialized_input_property,
                               size_t serialized_input_size,
                               void** serialized_property,
                               size_t* serialized_size);

  /**
   * @brief Release the serialized property.
   * @param[in] (key) Key of property.
   * @param[in] (serialized_property) Serialized property address by Get().
   * @param[in] (serialized_size) Serialized property size by Get().
   * @return Status object.
   */
  senscord::Status ReleaseProperty(const std::string& key,
                                   void* serialized_property,
                                   size_t serialized_size);

  /**
   * @brief Register the properties to created component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (key_list) The list of supported property keys.
   * @return Status object.
   */
  senscord::Status RegisterProperties(const std::string& port_type,
                                      int32_t port_id,
                                      const PropertyKeyList& key_list);

  /**
   * @brief Unregister the properties from component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @return Status object.
   */
  senscord::Status UnregisterProperties(const std::string& port_type,
                                        int32_t port_id);

  /**
   * @brief Add properties to component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (key_list) The list of supported property keys.
   * @return Status object.
   */
  senscord::Status AddProperties(const std::string& port_type, int32_t port_id,
                                 const PropertyKeyList& key_list);

  /**
   * @brief delete properties from component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (key_list) The list of supported property keys.
   * @return Status object.
   */
  senscord::Status DeleteProperties(const std::string& port_type,
                                    int32_t port_id,
                                    const PropertyKeyList& key_list);

  /**
   * @brief Send frame info.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (frameinfo) Information of the frame to send.
   * @return Status object.
   */
  senscord::Status SendFrame(int32_t port_id,
                             const senscord::FrameInfo& frameinfo);

  /**
   * @brief Update frame channel property.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (channel_id) Target channel ID.
   * @param[in] (key) Property key to updated.
   * @param[in] (prop) Property to updated.
   * @return Status object.
   */
  senscord::Status UpdateFrameProperty(int32_t port_id, uint32_t channel_id,
                                       const std::string& key,
                                       const senscord::BinaryProperty* prop);

  /**
   * @brief Update play position property.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (channel_id) Target channel ID.
   * @param[in] (position) Playback position.
   */
  void UpdatePlayPositionProperty(
      int32_t port_id, uint32_t channel_id, uint32_t position);

  /**
   * @brief Set stream type to component port.
   * @param[in] (port_type) The type of port.
   * @param[in] (port_id) The ID of port type.
   * @param[in] (type) Stream type.
   * @return Status object.
   */
  senscord::Status SetType(const std::string& port_type, int32_t port_id,
                           const std::string& type);

  /**
   * @brief Get target_path list from component port.
   * @param[out] (paths) target_path list.
   * @return Status object.
   */
  senscord::Status GetTargetPathList(
      std::map<int32_t, std::string>* paths);

 protected:
  senscord::ComponentPortManager* port_manager_;
  senscord::MemoryAllocator* allocator_;

  senscord::osal::OSMutex* mutex_;

  std::vector<senscord::ComponentPort*> play_port_list_;
  // supported properties
  PortPropertyKeyMap port_property_key_map_;

  std::map<int32_t, PlayerComponentPortData*> port_data_list_;
  PlayerSendIntervalManager* send_interval_manager_;

 private:
  struct PlayerComponentArgument {
    size_t composite_buffer_size;
  };
  PlayerComponentArgument component_argument_;

  /**
   * @brief Parse argument of internal parameter.
   * @param[in] (args) The arguments of component instance.
   */
  void PaserComponentInternalArgument(
      const senscord::ComponentArgument& args);

  /**
   * @brief Parse argument of port parameter.
   * @param[in] (args) The arguments of component instance.
   * @param[out] (ports) The list of port (key=port_id, vaue=port_type).
   */
  void ParseComponentPortArgument(
      const senscord::ComponentArgument& args,
      std::map<int32_t, std::string>* ports);

  /**
   * @brief find PlayerComponentPortData by port_id
   * @param[in] (port_id) port_id
   * @return PlayerComponentPortData*. if NULL, the data is not find
   */
  PlayerComponentPortData* FindPortData(int32_t port_id);

  /**
   * @brief Unregister the properties from component port.
   * @param[in] (port) The component port.
   * @param[in] (key_list) The list of supported property keys.
   */
  void UnregisterPortProperties(senscord::ComponentPort* port,
                                PropertyKeyList* key_list);

  /**
   * @brief Reset the playback position of all ports synchronous play.
   */
  void ResetSynchronousPlayPosition();
};

#endif  // LIB_COMPONENT_PLAYER_SRC_PLAYER_COMPONENT_H_
