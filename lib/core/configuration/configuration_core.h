/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CORE_CONFIGURATION_CONFIGURATION_CORE_H_
#define LIB_CORE_CONFIGURATION_CONFIGURATION_CORE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "core/internal_types.h"
#include "core/config_manager.h"
#include "senscord/configuration.h"
#include "senscord/osal.h"
#include "util/mutex.h"

namespace senscord {

/**
 * @brief Implementation of Configuration class for senscord core internal
 */
class ConfigurationCore : public Configuration {
 public:
  /**
   * @brief constructor.
   */
  ConfigurationCore();

  /**
   * @brief destructor.
   */
  virtual ~ConfigurationCore();

  /**
   * @brief Adds a stream.
   * @param[in] (stream_key) Stream key.
   * @param[in] (instance_name) Component instance name.
   * @param[in] (stream_type) Stream type.
   * @param[in] (port_id) Port id.
   * @return Status object.
   */
  virtual Status AddStream(
      const std::string& stream_key,
      const std::string& instance_name,
      const std::string& stream_type,
      int32_t port_id);

  /**
   * @brief Sets the buffering mode of the stream.
   * @param[in] (stream_key) Stream key.
   * @param[in] (buffering) Buffering "ON" or "OFF".
   * @param[in] (num) Buffering frame number.
   * @param[in] (format) Buffering format.
   * @return Status object.
   */
  virtual Status SetStreamBuffering(
      const std::string& stream_key,
      Buffering buffering,
      int32_t num,
      BufferingFormat format);

  /**
   * @brief Adds a stream argument.
   * @param[in] (stream_key) Stream key.
   * @param[in] (argument_name) Argument name.
   * @param[in] (argument_value) Argument value.
   * @return Status object.
   */
  virtual Status AddStreamArgument(
      const std::string& stream_key,
      const std::string& argument_name,
      const std::string& argument_value);

  /**
   * @brief Adds an instance.
   * @param[in] (instance_name) Component instance name.
   * @param[in] (component_name) Component library name.
   * @return Status object.
   */
  virtual Status AddInstance(
      const std::string& instance_name,
      const std::string& component_name);

  /**
   * @brief Adds an instance argument.
   * @param[in] (instance_name) Component instance name.
   * @param[in] (argument_name) Argument name.
   * @param[in] (argument_value) Argument value.
   * @return Status object.
   */
  virtual Status AddInstanceArgument(
      const std::string& instance_name,
      const std::string& argument_name,
      const std::string& argument_value);

  /**
   * @brief Adds an instance allocator.
   * @param[in] (instance_name) Component instance name.
   * @param[in] (allocator_key) Allocator key.
   * @param[in] (allocator_name) Allocator name.
   * @return Status object.
   */
  virtual Status AddInstanceAllocator(
      const std::string& instance_name,
      const std::string& allocator_key,
      const std::string& allocator_name);

  /**
   * @brief Adds an allocator.
   * @param[in] (allocator_key) Allocator key.
   * @param[in] (type) Allocator type.
   * @param[in] (cacheable) Cacheable or not.
   * @return Status object.
   */
  virtual Status AddAllocator(
      const std::string& allocator_key,
      const std::string& type,
      bool cacheable);

  /**
   * @brief Adds an allocator argument.
   * @param[in] (allocator_key) Allocator key.
   * @param[in] (argument_name) Argument name.
   * @param[in] (argument_value) Argument value.
   * @return Status object.
   */
  virtual Status AddAllocatorArgument(
      const std::string& allocator_key,
      const std::string& argument_name,
      const std::string& argument_value);

  /**
   * @brief Set the SensCord server search configuration.
   * @param[in] (type) Search type.
   * @param[in] (is_enabled) Enable or disable of specified search.
   * @param[in] (arguments) Arguments for the specified search.
   * @return Status object.
   */
  virtual Status SetSearch(const std::string& type, const bool is_enabled,
      const ConfigArgument* arguments);

  /**
   * @brief Get the SensCord server search configuration.
   * @param[in] (type) Search type.
   * @param[out] (is_enabled) The enable/disable status of specified search.
   * @param[out] (arguments) Arguments for the specified search.
   * @return Status object.
   */
  virtual Status GetSearch(
      const std::string& type, bool* is_enabled, ConfigArgument* arguments);

  /**
   * @brief Set the log level.
   * @param[in] (type) Log type.
   * @param[in] (severity) Log severity.
   * @return Status object.
   */
  virtual Status SetLogLevel(
      const std::string& type, LogLevel severity);

  /**
   * @brief Get the log level.
   * @param[in] (type) Log type.
   * @param[out] (severity) Log severity.
   * @return Status object.
   */
  virtual Status GetLogLevel(
      const std::string& type, LogLevel* severity);

  /**
   * @brief Get server list.
   * @param[out] server list. (Key=UID)
   * @return Status object.
   */
  virtual Status GetServerList(
      std::map<uint32_t, ConfigArgument>* servers);

  /**
   * @brief Add server.
   * @param[in] (arguments) Arguments for the specified server.
   * @param[out] UID.
   * @return Status object.
   */
  virtual Status AddServer(
      const ConfigArgument& arguments, uint32_t* uid);

  /**
   * @brief Remove server.
   * @param[in] UID.
   * @param[out] (arguments) Arguments for the specified server.
   * @return Status object.
   */
  virtual Status RemoveServer(
      const uint32_t& uid, ConfigArgument* arguments);

  /**
   * @brief Get configuration.
   */
  const CoreConfig& GetConfig() const;

  /**
   * @brief Initialize configuration.
   * @return Status object.
   */
  Status InitConfig();

 private:
#ifdef SENSCORD_SERVER_SETTING
  /**
   * @brief Set SearchSetting from argument
   * @param[out] (search) SearchSetting structure
   * @param[in] (type) Search type.
   * @param[in] (is_enabled) Enable or disable of specified search.
   * @param[in] (arguments) Arguments for the specified search.
   */
  void SetSearchSetting(SearchSetting* search, const std::string& type,
      const bool is_enabled, const ConfigArgument* arguments);

  /**
   * @brief Get server information.
   * @param[in] (server_setting) Server setting.
   * @param[out] (type) Connection type.
   * @param[out] (address) Destination address.
   * @return Status object.
   */
  Status GetServerInfo(
      const ConfigArgument& server_setting,
      std::string* type,
      std::string* address);

  /**
   * @brief Convert local_config to user server list.
   * @return Status object.
   */
  Status ConvertToUserServerList();

  /**
   * @brief Convert user server list to local_config server list.
   */
  void ConvertToLocalConfigServerList();
#endif  // SENSCORD_SERVER_SETTING

  // local configuration values
  CoreConfig local_config_;
  mutable util::Mutex mutex_;

#ifdef SENSCORD_SERVER_SETTING
  std::map<uint32_t, ConfigArgument> server_list_uid_;
#endif  // SENSCORD_SERVER_SETTING
};

}  // namespace senscord

#endif  // LIB_CORE_CONFIGURATION_CONFIGURATION_CORE_H_
