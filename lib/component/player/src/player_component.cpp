/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "player_component.h"

#include <inttypes.h>  // %"PRIu64"
#include <stdint.h>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "senscord/logger.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/property_types.h"
#include "senscord/serialize.h"
#include "./player_component_util.h"
#include "./player_component_port_data.h"
#include "./player_autolock.h"

namespace {
  const char* kModuleName = "player_component";

  // component argument
  const char* kArgumentPortPrefix = "port:";
  const char* kArgumentPortNum = "port_num";
  const char* kArgumentPortType = "port_type";
  const char* kArgumentCompositeBufferSize = "composite_buffer_size";

  // component argument value (default/min/max)
  const uint64_t kPortNumDefault = 5;
  const uint64_t kPortNumMax = 2147483647;  // INT_MAX
  const char* kPortTypeDefault ="play";
  const size_t kCompositeBufferSizeDefault = 104857600;   // default=100MB
  const size_t kCompositeBufferSizeMin     = 1048576;     // min=1MB
  const size_t kCompositeBufferSizeMax     = 1073741824;  // max=1GB
}  // namespace

/**
 * @brief Create component instance.
 * @return Created component instance. In case of failure, it returns NULL.
 */
extern "C" void* CreateComponent() { return new PlayerComponent(); }

/**
 * @brief Destroy component instance.
 * @param[in] component Instance created in CreateComponent().
 */
extern "C" void DestroyComponent(void* component) {
  delete reinterpret_cast<PlayerComponent*>(component);
}

/**
 * @brief Constructor.
 */
PlayerComponent::PlayerComponent()
    : port_manager_(NULL),
      allocator_(NULL),
      play_port_list_(),
      port_property_key_map_(),
      port_data_list_(),
      send_interval_manager_(NULL),
      component_argument_() {
  senscord::osal::OSCreateMutex(&mutex_);
}

/**
 * @brief Destructor.
 */
PlayerComponent::~PlayerComponent() {
  senscord::osal::OSDestroyMutex(mutex_);
  mutex_ = NULL;
  delete send_interval_manager_;
  send_interval_manager_ = NULL;
}

/**
 * @brief Parse argument of internal parameter.
 * @param[in] (args) The arguments of component instance.
 */
void PlayerComponent::PaserComponentInternalArgument(
    const senscord::ComponentArgument& args) {
  SENSCORD_LOG_DEBUG("Parse component internal argument.");

  // set default value (In case there is no argument)
  size_t buffer_size = kCompositeBufferSizeDefault;

  // parse composite_buffer_size
  SENSCORD_LOG_DEBUG("Parse \"composite_buffer_size\"");
  std::map<std::string, std::string>::const_iterator found;
  found = args.arguments.find(kArgumentCompositeBufferSize);
  if (found != args.arguments.end()) {
    SENSCORD_LOG_DEBUG(" - argument: [%s] %s",
        found->first.c_str(), found->second.c_str());
    uint64_t convert_value = 0;
    int32_t ret = senscord::osal::OSStrtoull(
        found->second.c_str(), NULL, 0, &convert_value);
    if ((ret != 0) ||
        (convert_value < kCompositeBufferSizeMin) ||
        (convert_value > kCompositeBufferSizeMax)) {
      // Set default value.
      // - In case of "convert error".
      // - In case of "value < minimum(1MB)".
      // - In case of "value > maximum(1GB)".
      convert_value = kCompositeBufferSizeDefault;
    }

    // Set buffer size from argument.
    buffer_size = static_cast<size_t>(convert_value);
  }

  // Apply composite buffer size
  component_argument_.composite_buffer_size = buffer_size;
}

/**
 * @brief Parse argument of port parameter.
 * @param[in] (args) The arguments of component instance.
 * @param[out] (ports) The list of port (key=port_id, vaue=port_type).
 */
void PlayerComponent::ParseComponentPortArgument(
    const senscord::ComponentArgument& args,
    std::map<int32_t, std::string>* ports) {
  SENSCORD_LOG_DEBUG("Parse component port argument.");

  // parse port:<integer>
  SENSCORD_LOG_DEBUG("Parse \"port:<integer>\"");
  size_t prefix_size = std::string(kArgumentPortPrefix).size();
  std::map<std::string, std::string>::const_iterator
      itr = args.arguments.begin(), end = args.arguments.end();
  for (; itr != end; ++itr) {
    SENSCORD_LOG_DEBUG(" - argument: [%s] %s",
        itr->first.c_str(), itr->second.c_str());

    // Check prefix
    if (itr->first.find(kArgumentPortPrefix) != 0) {
      SENSCORD_LOG_DEBUG("   -> not match prefix");
      continue;
    }

    // Pull out the <integer>
    size_t target_size = itr->first.size() - prefix_size;
    std::string target = itr->first.substr(prefix_size, target_size);
    // Check empty
    if (target.empty()) {
      SENSCORD_LOG_DEBUG("   -> <integer> is empty");
      continue;
    }

    // Check illegal character
    bool is_illegal_char = false;
    for (std::string::const_iterator
        str_itr = target.begin(), str_end = target.end();
        str_itr != str_end; ++str_itr) {
      if (*str_itr < '0' || *str_itr > '9') {
        is_illegal_char = true;
        break;
      }
    }
    if (is_illegal_char) {
      SENSCORD_LOG_DEBUG("   -> contains illegal characters");
      continue;
    }

    // Convert string to integer
    uint64_t convert_value = 0;
    int32_t ret = senscord::osal::OSStrtoull(
        target.c_str(), NULL, 0, &convert_value);
    if ((ret != 0) || (convert_value > kPortNumMax)) {
      SENSCORD_LOG_DEBUG("   -> can not convert <integer>");
      continue;
    }

    // Add port to list
    int32_t port_id = static_cast<int32_t>(convert_value);
    (*ports)[port_id] = itr->second;
  }

  if (ports->size() != 0) {
    return;  // OK
  }

  // set default value (In case there is no argument)
  int32_t port_num = kPortNumDefault;
  std::string port_type = kPortTypeDefault;

  // parse port_num
  SENSCORD_LOG_DEBUG("Parse \"port_num\"");
  std::map<std::string, std::string>::const_iterator found;
  found = args.arguments.find(kArgumentPortNum);
  if (found != args.arguments.end()) {
    SENSCORD_LOG_DEBUG(" - argument: [%s] %s",
        found->first.c_str(), found->second.c_str());
    uint64_t convert_value = 0;
    int32_t ret = senscord::osal::OSStrtoull(
        found->second.c_str(), NULL, 0, &convert_value);
    if ((ret != 0) || (convert_value == 0)) {
      // Set default value.
      // - In case of "convert error".
      // - In case of "value <= 0".
      convert_value = kPortNumDefault;
    } else if (convert_value > kPortNumMax) {
      // Set maximum value.
      // - In case of "value > maximum(int32_t)".
      convert_value = kPortNumMax;
    }

    // Apply the port_num.
    port_num = static_cast<int32_t>(convert_value);
  }

  // parse port_type
  SENSCORD_LOG_DEBUG("Parse \"port_type\"");
  found = args.arguments.find(kArgumentPortType);
  if (found != args.arguments.end()) {
    SENSCORD_LOG_DEBUG(" - argument: [%s] %s",
        found->first.c_str(), found->second.c_str());

    // Apply the port_type.
    port_type = found->second;
  }

  // Add port to list
  for (int32_t id = 0; id < port_num; ++id) {
    (*ports)[id] = port_type;
  }
}

/**
 * @brief Implemented initialization.
 */
senscord::Status PlayerComponent::InitComponent(
    senscord::ComponentPortManager* port_manager,
    const senscord::ComponentArgument& args) {
  SENSCORD_LOG_DEBUG("Init PlayComponent.");

  // keep the port manager and allocator
  port_manager_ = port_manager;
  allocator_ = args.allocators.begin()->second;
  send_interval_manager_ = new PlayerSendIntervalManager();

  // Parse argument of internal parameter
  // - composite_buffer_size
  PaserComponentInternalArgument(args);

  // Parse argument of port parameter
  // - port:<integer>
  // - port_num
  // - port_type
  std::map<int32_t, std::string> ports;
  ParseComponentPortArgument(args, &ports);

  // Create player port instance
  for (std::map<int32_t, std::string>::iterator
      itr = ports.begin(), end = ports.end();
      itr != end; ++itr) {
    senscord::ComponentPort* component_port = NULL;
    senscord::Status status = port_manager->CreatePort(
        itr->second, itr->first, &component_port);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
    play_port_list_.push_back(component_port);
  }

  return senscord::Status::OK();
}

/**
 * @brief Implemented exit.
 */
senscord::Status PlayerComponent::ExitComponent() {
  return senscord::Status::OK();
}

/**
 * @brief Implemented open the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (args) The arg of component port.
 * @return Status object.
 */
senscord::Status PlayerComponent::OpenPort(
    const std::string& port_type, int32_t port_id,
    const senscord::ComponentPortArgument& args) {
  SENSCORD_LOG_DEBUG("Open PlayPort: %s.%" PRId32,
      port_type.c_str(), port_id);

  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    p_port_data = new PlayerComponentPortData(
        port_id, this, allocator_, send_interval_manager_);
    port_data_list_.insert(std::make_pair(port_id, p_port_data));
    status = p_port_data->OpenPort(port_type, port_id,
        component_argument_.composite_buffer_size, args);
    if (status.ok()) {
      ResetSynchronousPlayPosition();
    } else {
      SENSCORD_STATUS_TRACE(status);
      port_data_list_.erase(port_id);
      delete p_port_data;
      p_port_data = NULL;
    }
  } else {
    status = SENSCORD_STATUS_FAIL(kModuleName,
                                  senscord::Status::kCauseInvalidArgument,
                                  "already exists port_id=%" PRId32, port_id);
  }
  return status;
}

/**
 * @brief Implemented close the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponent::ClosePort(
    const std::string& port_type, int32_t port_id) {
  SENSCORD_LOG_DEBUG("Close PlayPort: %s.%" PRId32,
      port_type.c_str(), port_id);

  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    status = SENSCORD_STATUS_FAIL(kModuleName,
                                  senscord::Status::kCauseInvalidArgument,
                                  "invalid port_id=%" PRId32, port_id);
  } else {
    status = p_port_data->ClosePort(port_type, port_id);
    if (status.ok()) {
      port_data_list_.erase(port_id);
      delete p_port_data;
      p_port_data = NULL;
    }
  }

  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Start the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponent::StartPort(
    const std::string& port_type, int32_t port_id) {
  SENSCORD_LOG_DEBUG("Start PlayPort: %s.%" PRId32,
      port_type.c_str(), port_id);
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    return SENSCORD_STATUS_FAIL(kModuleName,
                                senscord::Status::kCauseInvalidArgument,
                                "invalid port_id=%" PRId32, port_id);
  }

  senscord::Status status = p_port_data->StartPort(port_type, port_id);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  return senscord::Status::OK();
}

/**
 * @brief Stop the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponent::StopPort(
    const std::string& port_type, int32_t port_id) {
  SENSCORD_LOG_DEBUG("Stop PlayPort: %s.%" PRId32,
      port_type.c_str(), port_id);
  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    return SENSCORD_STATUS_FAIL(kModuleName,
                                senscord::Status::kCauseInvalidArgument,
                                "invalid port_id=%" PRId32, port_id);
  }
  status = p_port_data->StopPort(port_type, port_id);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  return senscord::Status::OK();
}

/**
 * @brief Release the frame pushed from the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frameinfo) Information to release frame.
 * @param[in] (referenced_channel_ids) List of referenced channel IDs.
 * @return Status object.
 */
senscord::Status PlayerComponent::ReleasePortFrame(
    const std::string& port_type, int32_t port_id,
    const senscord::FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids) {
  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    status = SENSCORD_STATUS_FAIL(kModuleName,
        senscord::Status::kCauseInvalidArgument,
        "invalid port_id=%" PRId32, port_id);
  } else {
    p_port_data->ReleasePortFrame(frameinfo);
  }
  return status;
}

/**
 * @brief find PlayerComponentPortData by port_id
 * @param[in] (port_id) port_id
 * @return PlayerComponentPortData*. if NULL, the data is not find
 */
PlayerComponentPortData* PlayerComponent::FindPortData(int32_t port_id) {
  PlayerComponentPortData* p_find = NULL;
  std::map<int32_t, PlayerComponentPortData*>::iterator itr =
      port_data_list_.find(port_id);
  if (itr != port_data_list_.end()) {
    p_find = itr->second;
  }
  return p_find;
}

/**
 * @brief Set the serialized property.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (key) Key of property.
 * @param[in] (serialized_property) Serialized property address.
 * @param[in] (serialized_size) Serialized property size.
 * @return Status object.
 */
senscord::Status PlayerComponent::SetProperty(const std::string& port_type,
                                              int32_t port_id,
                                              const std::string& key,
                                              const void* serialized_property,
                                              size_t serialized_size) {
  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    status = SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "invalid port_id=%" PRId32, port_id);
  } else {
    status = p_port_data->SetProperty(
        port_type, port_id, key, serialized_property, serialized_size);

    if (status.ok() && (key == senscord::kPlayPropertyKey)) {
      ResetSynchronousPlayPosition();
    }
  }
  return status;
}

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
senscord::Status PlayerComponent::GetProperty(
    const std::string& port_type, int32_t port_id, const std::string& key,
    const void* serialized_input_property, size_t serialized_input_size,
    void** serialized_property, size_t* serialized_size) {
  senscord::Status status;
  player::AutoLock autolock(mutex_);
  PlayerComponentPortData* p_port_data = FindPortData(port_id);
  if (p_port_data == NULL) {
    status = SENSCORD_STATUS_FAIL(kModuleName,
                                  senscord::Status::kCauseInvalidArgument,
                                  "invalid port_id=%" PRId32, port_id);
  } else {
    status = p_port_data->GetProperty(
        port_type, port_id, key, serialized_input_property,
        serialized_input_size, serialized_property, serialized_size);
  }

  return status;
}

/**
 * @brief Release the serialized property.
 * @param[in] (key) Key of property.
 * @param[in] (serialized_property) Serialized property address by Get().
 * @param[in] (serialized_size) Serialized property size by Get().
 * @return Status object.
 */
senscord::Status PlayerComponent::ReleaseProperty(const std::string& key,
                                                  void* serialized_property,
                                                  size_t serialized_size) {
  if (serialized_size == 0) {
    // do nothing
    return senscord::Status::OK();
  }
  if (serialized_property == NULL) {
    return SENSCORD_STATUS_FAIL(kModuleName,
                                senscord::Status::kCauseInvalidArgument,
                                "parameter is null");
  }
  delete[] reinterpret_cast<uint8_t*>(serialized_property);
  return senscord::Status::OK();
}

/**
 * @brief Register the properties to created component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (key_list) The list of supported property keys.
 * @return Status object.
 */
senscord::Status PlayerComponent::RegisterProperties(
    const std::string& port_type, int32_t port_id,
    const PropertyKeyList& key_list) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  // register to self map
  PropertyKeyList* new_key_list = new PropertyKeyList();
  port_property_key_map_.insert(std::make_pair(port, new_key_list));

  // register to port
  senscord::Status status;
  PropertyKeyList::const_iterator itr = key_list.begin();
  PropertyKeyList::const_iterator end = key_list.end();
  for (; itr != end; ++itr) {
    std::string key = (*itr);
    SENSCORD_LOG_DEBUG("[player] found property: %s", key.c_str());

    // register
    senscord::PropertyAccessor* accessor =
        new player::PlayerPropertyAccessor(key, this, port_type, port_id);
    status = port->RegisterPropertyAccessor(accessor);
    if (!status.ok()) {
      delete accessor;
      UnregisterProperties(port_type, port_id);
      return SENSCORD_STATUS_TRACE(status);
    }
    new_key_list->push_back(key);
  }
  return senscord::Status::OK();
}

/**
 * @brief Unregister the properties from component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status PlayerComponent::UnregisterProperties(
    const std::string& port_type, int32_t port_id) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  // remove from map
  PortPropertyKeyMap::iterator itr = port_property_key_map_.find(port);
  if (itr == port_property_key_map_.end()) {
    return SENSCORD_STATUS_FAIL(kModuleName, senscord::Status::kCauseNotFound,
                                "no registered properties: %s, %" PRId32,
                                port_type.c_str(), port_id);
  }
  PropertyKeyList* key_list = itr->second;
  port_property_key_map_.erase(itr);

  // unregister for all properties
  UnregisterPortProperties(port, key_list);
  delete key_list;

  return senscord::Status::OK();
}

/**
 * @brief Add properties to component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (key_list) The list of supported property keys.
 * @return Status object.
 */
senscord::Status PlayerComponent::AddProperties(
    const std::string& port_type, int32_t port_id,
    const PropertyKeyList& key_list) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  PortPropertyKeyMap::iterator itr = port_property_key_map_.find(port);
  if (itr == port_property_key_map_.end()) {
    return SENSCORD_STATUS_FAIL(kModuleName, senscord::Status::kCauseNotFound,
                                "no registered properties: %s, %" PRId32,
                                port_type.c_str(), port_id);
  }
  PropertyKeyList* dst_key_list = itr->second;

  // add to port
  senscord::Status status;
  PropertyKeyList::const_iterator itr1 = key_list.begin();
  PropertyKeyList::const_iterator end1 = key_list.end();
  for (; itr1 != end1; ++itr1) {
    std::string key = (*itr1);
    SENSCORD_LOG_DEBUG("[player] found property: %s", key.c_str());

    // register
    senscord::PropertyAccessor* accessor =
        new player::PlayerPropertyAccessor(key, this, port_type, port_id);
    status = port->RegisterPropertyAccessor(accessor);
    if (!status.ok()) {
      delete accessor;
      SENSCORD_LOG_WARNING("[player] registration failed: %s",
          status.ToString().c_str());
      continue;
    }
    dst_key_list->push_back(key);
  }
  return senscord::Status::OK();
}

/**
 * @brief delete properties from component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (key_list) The list of supported property keys.
 * @return Status object.
 */
senscord::Status PlayerComponent::DeleteProperties(
    const std::string& port_type, int32_t port_id,
    const PropertyKeyList& key_list) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  PortPropertyKeyMap::iterator itr = port_property_key_map_.find(port);
  if (itr == port_property_key_map_.end()) {
    return SENSCORD_STATUS_FAIL(kModuleName, senscord::Status::kCauseNotFound,
                                "no registered properties: %s, %" PRId32,
                                port_type.c_str(), port_id);
  }
  PropertyKeyList* dst_key_list = itr->second;

  // add to port
  senscord::Status status;
  PropertyKeyList::const_iterator itr1 = key_list.begin();
  PropertyKeyList::const_iterator end1 = key_list.end();
  for (; itr1 != end1; ++itr1) {
    std::string key = (*itr1);
    SENSCORD_LOG_DEBUG("[player] found property: %s", key.c_str());

    PropertyKeyList::iterator itr2 = dst_key_list->begin();
    PropertyKeyList::iterator end2 = dst_key_list->end();
    while (itr2 != end2) {
      if (*itr2 == key) {
        senscord::PropertyAccessor* accessor = NULL;
        port->UnregisterPropertyAccessor(*itr2, &accessor);
        delete accessor;
        dst_key_list->erase(itr2);
        break;
      } else {
        ++itr2;
      }
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Unregister the properties from component port.
 * @param[in] (port) The component port.
 * @param[in] (key_list) The list of supported property keys.
 */
void PlayerComponent::UnregisterPortProperties(senscord::ComponentPort* port,
                                               PropertyKeyList* key_list) {
  if ((port) && (key_list)) {
    PropertyKeyList::iterator itr = key_list->begin();
    PropertyKeyList::const_iterator end = key_list->end();
    for (; itr != end; ++itr) {
      senscord::PropertyAccessor* accessor = NULL;
      port->UnregisterPropertyAccessor(*itr, &accessor);
      delete accessor;
    }
  }
}

/**
 * @brief Send frame info.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frameinfo) Information of the frame to send.
 * @return Status object.
 */
senscord::Status PlayerComponent::SendFrame(
    int32_t port_id, const senscord::FrameInfo& frameinfo) {
  senscord::Status status;
  if (port_id > static_cast<int32_t>(play_port_list_.size() - 1) ||
      port_id < 0) {
    return SENSCORD_STATUS_FAIL(kModuleName,
                                senscord::Status::kCauseInvalidArgument,
                                "invalid port_id=%" PRId32, port_id);
  }
  status = play_port_list_[port_id]->SendFrame(frameinfo);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Update frame channel property.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (channel_id) Target channel ID.
 * @param[in] (key) Property key to updated.
 * @param[in] (prop) Property to updated.
 * @return Status object.
 */
senscord::Status PlayerComponent::UpdateFrameProperty(
    int32_t port_id, uint32_t channel_id, const std::string& key,
    const senscord::BinaryProperty* prop) {
  senscord::Status status;

  if (prop == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument, "prop is NULL");
  }
  if (port_id > static_cast<int32_t>(play_port_list_.size() - 1) ||
      port_id < 0) {
    return SENSCORD_STATUS_FAIL(kModuleName,
                                senscord::Status::kCauseInvalidArgument,
                                "invalid port_id=%" PRId32, port_id);
  }
  status =
      play_port_list_[port_id]->UpdateFrameProperty(channel_id, key, prop);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Update play position property.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (channel_id) Target channel ID.
 * @param[in] (position) Playback position.
 */
void PlayerComponent::UpdatePlayPositionProperty(
    int32_t port_id, uint32_t channel_id, uint32_t position) {
  if (port_id >= 0 && port_id < static_cast<int32_t>(play_port_list_.size())) {
    senscord::PlayPositionProperty prop = {};
    prop.position = position;
    senscord::Status status = play_port_list_[port_id]->UpdateFrameProperty(
        channel_id, senscord::kPlayPositionPropertyKey, &prop);
    if (!status.ok()) {
      SENSCORD_LOG_WARNING(
          "[%s] UpdateFrameProperty NG(%s).",
          kModuleName, status.ToString().c_str());
    }
  }
}

/**
 * @brief Set stream type to component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (type) Stream type.
 * @return Status object.
 */
senscord::Status PlayerComponent::SetType(const std::string& port_type,
                                          int32_t port_id,
                                          const std::string& type) {
  senscord::Status status;
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument, "port is NULL");
  }
  SENSCORD_LOG_INFO("SetType() %s", type.c_str());
  status = port->SetType(type);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get target_path list from component port.
 * @param[out] (paths) target_path list.
 * @return Status object.
 */
senscord::Status PlayerComponent::GetTargetPathList(
    std::map<int32_t, std::string>* paths) {
  senscord::Status status;

  if (paths == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument, "paths is NULL");
  }

  std::map<int32_t, PlayerComponentPortData*>::iterator itr;
  for (itr = port_data_list_.begin(); itr != port_data_list_.end(); ++itr) {
    const std::string& path = itr->second->GetTargetPath();
    paths->insert(std::make_pair(itr->first, path));
  }
  return senscord::Status::OK();
}

/**
 * @brief Reset the playback position of all ports synchronous play.
 */
void PlayerComponent::ResetSynchronousPlayPosition() {
  std::map<int32_t, PlayerComponentPortData*>::iterator itr;
  if (send_interval_manager_->GetSendManagePortCount() > 1) {
    for (itr = port_data_list_.begin();
        itr != port_data_list_.end(); ++itr) {
      itr->second->SetPlayStartPosition(0);   // correct start_offset
    }
  }
}
