/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "configuration/configuration_core.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <utility>

#include "configuration/core_config.h"
#include "configuration/allocator_config_reader.h"
#include "senscord/configuration.h"
#include "senscord/osal.h"
#include "senscord/status.h"
#include "senscord/environment.h"
#include "util/senscord_utils.h"
#include "util/autolock.h"
#include "util/mutex.h"

// #define CONFIGURATION_CORE_DEBUG

// printf is used because LogSeverity is not set when this class works.
#ifdef CONFIGURATION_CORE_DEBUG
#ifdef _WIN32
#define CONFIG_API_LOG_DEBUG(x, ...) osal::OSPrintf(x "\r\n", __VA_ARGS__)
#else
#define CONFIG_API_LOG_DEBUG(x, ...) osal::OSPrintf(x "\r\n", ##__VA_ARGS__)
#endif
#else
#define CONFIG_API_LOG_DEBUG(x, ...)
#endif

namespace {

const char kArgumentConnection[] = "connection";
const char kArgumentAddress[] = "address";

const uint16_t kConfigServerMax = 1000;

}  // namespace

namespace senscord {

/**
 * @brief Create Configuration instance.
 * @param[out] Configuration pointer.
 * @return Status object.
 */
Status Configuration::Create(Configuration** config) {
  SENSCORD_STATUS_ARGUMENT_CHECK(config == NULL);

  ConfigurationCore* tmp_config = new ConfigurationCore();
  Status status = tmp_config->InitConfig();
  if (!status.ok()) {
    delete tmp_config;
    *config = NULL;
    return SENSCORD_STATUS_TRACE(status);
  }
  *config = tmp_config;

  return Status::OK();
}

/**
 * @brief Delete Configuration instance.
 * @param[in] (configuration) Configuration object.
 */
void Configuration::Delete(Configuration* config) {
  delete config;
}

/**
 * @brief ConfigurationCore constructor.
 */
ConfigurationCore::ConfigurationCore() : local_config_() {
}

/**
 * @brief ConfigurationCore destructor.
 */
ConfigurationCore::~ConfigurationCore() {
}

/**
 * @brief Adds a stream.
 * @param[in] (stream_key) Stream key.
 * @param[in] (instance_name) Component instance name.
 * @param[in] (stream_type) Stream type.
 * @param[in] (port_id) Port id.
 * @return Status object.
 */
Status ConfigurationCore::AddStream(
    const std::string& stream_key,
    const std::string& instance_name,
    const std::string& stream_type,
    int32_t port_id) {
  SENSCORD_STATUS_ARGUMENT_CHECK(stream_key.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(instance_name.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(stream_type.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(port_id < 0);

  util::AutoLock lock(&mutex_);
  if (GetStreamConfig(&local_config_.stream_list, stream_key) != NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAlreadyExists,
        "stream already exists. key=%s", stream_key.c_str());
  }
  StreamSetting stream_setting;
  stream_setting.stream_key = stream_key;
  stream_setting.address.instance_name = instance_name;
  stream_setting.address.port_type = stream_type;
  stream_setting.address.port_id = port_id;
#ifdef SENSCORD_SERVER
  stream_setting.radical_address = stream_setting.address;
#endif  // SENSCORD_SERVER
  local_config_.stream_list.push_back(stream_setting);
  return Status::OK();
}

/**
 * @brief Sets the buffering mode of the stream.
 * @param[in] (stream_key) Stream key.
 * @param[in] (buffering) Buffering "ON" or "OFF".
 * @param[in] (num) Buffering frame number.
 * @param[in] (format) Buffering format.
 * @return Status object.
 */
Status ConfigurationCore::SetStreamBuffering(
    const std::string& stream_key,
    Buffering buffering,
    int32_t num,
    BufferingFormat format) {
  SENSCORD_STATUS_ARGUMENT_CHECK(stream_key.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(
      buffering < kBufferingDefault || buffering > kBufferingOn);
  SENSCORD_STATUS_ARGUMENT_CHECK(num < kBufferNumDefault);
  SENSCORD_STATUS_ARGUMENT_CHECK(
      format < kBufferingFormatDefault || format > kBufferingFormatOverwrite);

  util::AutoLock lock(&mutex_);
  StreamSetting* stream_setting =
      GetStreamConfig(&local_config_.stream_list, stream_key);
  if (stream_setting == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "stream not found. key=%s", stream_key.c_str());
  }
  stream_setting->frame_buffering.buffering = buffering;
  stream_setting->frame_buffering.num = num;
  stream_setting->frame_buffering.format = format;
  return Status::OK();
}

/**
 * @brief Adds a stream argument.
 * @param[in] (stream_key) Stream key.
 * @param[in] (argument_name) Argument name.
 * @param[in] (argument_value) Argument value.
 * @return Status object.
 */
Status ConfigurationCore::AddStreamArgument(
    const std::string& stream_key,
    const std::string& argument_name,
    const std::string& argument_value) {
  SENSCORD_STATUS_ARGUMENT_CHECK(stream_key.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(argument_name.empty());

  util::AutoLock lock(&mutex_);
  StreamSetting* stream_setting =
      GetStreamConfig(&local_config_.stream_list, stream_key);
  if (stream_setting == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "stream not found. key=%s", stream_key.c_str());
  }
  stream_setting->arguments[argument_name] = argument_value;
  return Status::OK();
}

/**
 * @brief Adds an instance.
 * @param[in] (instance_name) Component instance name.
 * @param[in] (component_name) Component library name.
 * @return Status object.
 */
Status ConfigurationCore::AddInstance(
    const std::string& instance_name,
    const std::string& component_name) {
  SENSCORD_STATUS_ARGUMENT_CHECK(instance_name.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(component_name.empty());

  util::AutoLock lock(&mutex_);
  if (GetComponentConfig(
      &local_config_.instance_list, instance_name) != NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAlreadyExists,
        "instance already exists. name=%s", instance_name.c_str());
  }
  ComponentInstanceConfig config = {};
  config.instance_name = instance_name;
  config.component_name = component_name;
  local_config_.instance_list.push_back(config);
  return Status::OK();
}

/**
 * @brief Adds an instance argument.
 * @param[in] (instance_name) Component instance name.
 * @param[in] (argument_name) Argument name.
 * @param[in] (argument_value) Argument value.
 * @return Status object.
 */
Status ConfigurationCore::AddInstanceArgument(
    const std::string& instance_name,
    const std::string& argument_name,
    const std::string& argument_value) {
  SENSCORD_STATUS_ARGUMENT_CHECK(instance_name.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(argument_name.empty());

  util::AutoLock lock(&mutex_);
  ComponentInstanceConfig* config =
      GetComponentConfig(&local_config_.instance_list, instance_name);
  if (config == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "instance not found. name=%s", instance_name.c_str());
  }
  config->arguments[argument_name] = argument_value;
  return Status::OK();
}

/**
 * @brief Adds an instance allocator.
 * @param[in] (instance_name) Component instance name.
 * @param[in] (allocator_key) Allocator key.
 * @param[in] (allocator_name) Allocator name.
 * @return Status object.
 */
Status ConfigurationCore::AddInstanceAllocator(
    const std::string& instance_name,
    const std::string& allocator_key,
    const std::string& allocator_name) {
  SENSCORD_STATUS_ARGUMENT_CHECK(instance_name.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(allocator_key.empty());

  util::AutoLock lock(&mutex_);
  ComponentInstanceConfig* config =
      GetComponentConfig(&local_config_.instance_list, instance_name);
  if (config == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "instance not found. name=%s", instance_name.c_str());
  }
  std::string key = allocator_key;
  if (key == senscord::kDefaultAllocatorKey) {
    key = "";  // default allocator
  }
  config->allocator_key_list.insert(
      std::make_pair(allocator_name, key));
  return Status::OK();
}

/**
 * @brief Adds an allocator.
 * @param[in] (allocator_key) Allocator key.
 * @param[in] (type) Allocator type.
 * @param[in] (cacheable) Cacheable or not.
 * @return Status object.
 */
Status ConfigurationCore::AddAllocator(
    const std::string& allocator_key,
    const std::string& type,
    bool cacheable) {
  SENSCORD_STATUS_ARGUMENT_CHECK(allocator_key.empty());

  util::AutoLock lock(&mutex_);
  if (GetAllocatorConfig(
      &local_config_.allocator_list, allocator_key) != NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAlreadyExists,
        "allocator already exists. key=%s", allocator_key.c_str());
  }
  AllocatorConfig config = {};
  config.key = allocator_key;
  config.type = type;
  if (config.type.empty()) {
    config.type = kAllocatorTypeHeap;
  }
  config.cacheable = cacheable;
  local_config_.allocator_list.push_back(config);
  return Status::OK();
}

/**
 * @brief Adds an allocator argument.
 * @param[in] (allocator_key) Allocator key.
 * @param[in] (argument_name) Argument name.
 * @param[in] (argument_value) Argument value.
 * @return Status object.
 */
Status ConfigurationCore::AddAllocatorArgument(
    const std::string& allocator_key,
    const std::string& argument_name,
    const std::string& argument_value) {
  SENSCORD_STATUS_ARGUMENT_CHECK(allocator_key.empty());
  SENSCORD_STATUS_ARGUMENT_CHECK(argument_name.empty());

  util::AutoLock lock(&mutex_);
  AllocatorConfig* config =
      GetAllocatorConfig(&local_config_.allocator_list, allocator_key);
  if (config == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseNotFound,
        "allocator not found. key=%s", allocator_key.c_str());
  }
  config->arguments[argument_name] = argument_value;
  return Status::OK();
}

#ifdef SENSCORD_SERVER_SETTING
/**
 * @brief Set SearchSetting from argument
 * @param[out] (search) SearchSetting structure
 * @param[in] (type) Search type.
 * @param[in] (is_enabled) Enable or disable of specified search.
 * @param[in] (arguments) Arguments for the specified search.
 */
void ConfigurationCore::SetSearchSetting(SearchSetting* search,
    const std::string& type, const bool is_enabled,
    const ConfigArgument* arguments) {
  search->name = type;
  search->is_enabled = is_enabled;

  if (arguments) {
    search->arguments = *arguments;
  } else {
    search->arguments.clear();
  }

  return;
}
#endif  // SENSCORD_SERVER_SETTING

/**
 * @brief Set the SensCord server search configuration.
 * @param[in] (type) Search type.
 * @param[in] (is_enabled) Enable or disable of specified search.
 * @param[in] (arguments) Arguments for the specified search.
 * @return Status object.
 */
Status ConfigurationCore::SetSearch(const std::string& type,
    const bool is_enabled, const ConfigArgument* arguments) {
#ifndef SENSCORD_SERVER_SETTING
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "feature is disabled. (SENSCORD_SERVER_SETTING=OFF)");
#else
#ifndef SENSCORD_SERVER_SEARCH_SSDP
  if (type == kSearchTypeSsdp) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
        "feature is disabled. (SENSCORD_SERVER_SEARCH_SSDP=OFF)");
  }
#endif
#ifndef SENSCORD_SERVER_SEARCH_UCOM
  if (type == kSearchTypeUcom) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
        "feature is disabled. (SENSCORD_SERVER_SEARCH_UCOM=OFF)");
  }
#endif
  bool moded = false;
  util::AutoLock lock(&mutex_);

  for (std::vector<SearchSetting>::iterator
           itr = local_config_.search_list.begin(),
           end = local_config_.search_list.end();
       itr != end; ++itr) {
    SearchSetting* search = &(*itr);
    if (search->name == type) {
      SetSearchSetting(search, type, is_enabled, arguments);

      moded = true;
      CONFIG_API_LOG_DEBUG("[SetSearch] type found. moded. %s %d %p",
          type.c_str(), is_enabled, arguments);
    }
  }

  if (!moded) {
    SearchSetting tmp = {};

    SetSearchSetting(&tmp, type, is_enabled, arguments);

    local_config_.search_list.push_back(tmp);
    CONFIG_API_LOG_DEBUG("[SetSearch] type not found. added. %s %d %p",
        type.c_str(), is_enabled, arguments);
  }

  return Status::OK();
#endif  // SENSCORD_SERVER_SETTING
}

/**
 * @brief Get the SensCord server search configuration.
 * @param[in] (type) Search type.
 * @param[out] (is_enabled) The enable/disable status of specified search.
 * @param[out] (arguments) Arguments for the specified search.
 * @return Status object.
 */
Status ConfigurationCore::GetSearch(
    const std::string& type, bool* is_enabled, ConfigArgument* arguments) {
#ifndef SENSCORD_SERVER_SETTING
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "feature is disabled. (SENSCORD_SERVER_SETTING=OFF)");
#else
#ifndef SENSCORD_SERVER_SEARCH_SSDP
  if (type == kSearchTypeSsdp) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
        "feature is disabled. (SENSCORD_SERVER_SEARCH_SSDP=OFF)");
  }
#endif
#ifndef SENSCORD_SERVER_SEARCH_UCOM
  if (type == kSearchTypeUcom) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
        "feature is disabled. (SENSCORD_SERVER_SEARCH_UCOM=OFF)");
  }
#endif
  SENSCORD_STATUS_ARGUMENT_CHECK(is_enabled == NULL);

  util::AutoLock lock(&mutex_);
  for (std::vector<SearchSetting>::const_iterator
           itr = local_config_.search_list.begin(),
           end = local_config_.search_list.end();
       itr != end; ++itr) {
    if (itr->name == type) {
      *is_enabled = itr->is_enabled;

      if (arguments) {
        (*arguments) = itr->arguments;
      }

      return Status::OK();
    }
  }

  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotFound,
      "type \"%s\" not found", type.c_str());
#endif  // SENSCORD_SERVER_SETTING
}

/**
 * @brief Set the log level.
 * @param[in] (tag) Logger tag name.
 * @param[in] (level) Log level.
 * @return Status object.
 */
Status ConfigurationCore::SetLogLevel(
    const std::string& tag, LogLevel level) {
  SENSCORD_STATUS_ARGUMENT_CHECK(tag.empty());

#ifdef SENSCORD_LOG_ENABLED
  util::AutoLock lock(&mutex_);
  std::string set_tag = tag;
  if (tag == kLogSeverityTypeInstance) {
    set_tag = util::kLoggerTagDefault;
  }

  local_config_.tag_logger_list[set_tag] = level;
#endif  // SENSCORD_LOG_ENABLED

  return Status::OK();
}

/**
 * @brief Get the log level.
 * @param[in] (tag) Logger tag name.
 * @param[out] (level) Log level.
 * @return Status object.
 */
Status ConfigurationCore::GetLogLevel(
    const std::string& tag, LogLevel* level) {
  SENSCORD_STATUS_ARGUMENT_CHECK(level == NULL);
  SENSCORD_STATUS_ARGUMENT_CHECK(tag.empty());

#ifdef SENSCORD_LOG_ENABLED
  util::AutoLock lock(&mutex_);
  std::string search_tag = tag;
  if (tag == kLogSeverityTypeInstance) {
    search_tag = util::kLoggerTagDefault;
  }

  *level = kLogInfo;
  std::map<std::string, LogLevel>::const_iterator itr =
      local_config_.tag_logger_list.find(search_tag);
  if (itr != local_config_.tag_logger_list.end()) {
    *level = itr->second;
  }
#else
  *level = kLogOff;
#endif  // SENSCORD_LOG_ENABLED
  return Status::OK();
}

/**
 * @brief Get server list.
 * @param[out] server list. (Key=UID)
 * @return Status object.
 */
Status ConfigurationCore::GetServerList(
    std::map<uint32_t, ConfigArgument>* servers) {
  SENSCORD_STATUS_ARGUMENT_CHECK(servers == NULL);

#ifdef SENSCORD_SERVER_SETTING
  util::AutoLock lock(&mutex_);
  *servers = server_list_uid_;
#endif  // SENSCORD_SERVER_SETTING

  return Status::OK();
}

#ifdef SENSCORD_SERVER_SETTING
/**
 * @brief Get server information.
 * @param[in] (server_setting) Server setting.
 * @param[out] (type) Connection type.
 * @param[out] (address) Destination address.
 * @return Status object.
 */
Status ConfigurationCore::GetServerInfo(
    const ConfigArgument& server_setting,
    std::string* type,
    std::string* address) {
  typedef std::map<std::string, std::string>::const_iterator Iter;
  Iter end = server_setting.end();
  Iter type_itr = server_setting.find(kArgumentConnection);
  if (type_itr == end) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidArgument,
        "'%s' argument not specified", kArgumentConnection);
  }
  Iter addr_itr = server_setting.find(kArgumentAddress);
  if (addr_itr == end) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidArgument,
        "'%s' argument not specified", kArgumentAddress);
  }

  *type = type_itr->second;
  *address = addr_itr->second;

  return Status::OK();
}
#endif  // SENSCORD_SERVER_SETTING

/**
 * @brief Add server.
 * @param[in] (arguments) Arguments for the specified server.
 * @param[out] UID.
 * @return Status object.
 */
Status ConfigurationCore::AddServer(
    const ConfigArgument& arguments, uint32_t* uid) {
#ifdef SENSCORD_SERVER_SETTING
  std::string target_type;
  std::string target_address;
  uint16_t random_val = 0;

  util::AutoLock lock(&mutex_);

  // check list size
  if (server_list_uid_.size() >= kConfigServerMax) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore,
        Status::kCauseResourceExhausted,
        "the number of server settings exceeds the upper limit");
  }

  // check input arguments
  Status status = GetServerInfo(arguments, &target_type, &target_address);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  bool uid_generate = false;
  while (!uid_generate) {
    int32_t result = osal::OSRand(&random_val);
    if (result != 0) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseAborted,
          "failed to generate random value. (result=%" PRIx32 ")", result);
    }
    typedef std::map<uint32_t, ConfigArgument>::const_iterator Iter;
    Iter uid_itr = server_list_uid_.find(random_val);
    Iter end = server_list_uid_.end();
    if (uid_itr != end) {
      continue;
    }
    if (uid) {
      *uid = random_val;
    }
    server_list_uid_[random_val] = arguments;
    uid_generate = true;
  }

  ConvertToLocalConfigServerList();

  return Status::OK();
#else
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "feature is disabled. (SENSCORD_SERVER_SETTING=OFF)");
#endif  // SENSCORD_SERVER_SETTING
}

/**
 * @brief Remove server.
 * @param[in] UID.
 * @param[out] (arguments) Arguments for the specified server.
 * @return Status object.
 */
Status ConfigurationCore::RemoveServer(
    const uint32_t& uid, ConfigArgument* arguments) {
#ifdef SENSCORD_SERVER_SETTING
  util::AutoLock lock(&mutex_);
  typedef std::map<uint32_t, ConfigArgument>::const_iterator Iter;
  Iter uid_itr = server_list_uid_.find(uid);
  Iter end = server_list_uid_.end();
  if (uid_itr == end) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotFound,
                                "not found uid");
  }

  if (arguments) {
    *(arguments) = server_list_uid_[uid];
  }
  server_list_uid_.erase(uid);

  ConvertToLocalConfigServerList();

  return Status::OK();
#else
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "feature is disabled. (SENSCORD_SERVER_SETTING=OFF)");
#endif  // SENSCORD_SERVER_SETTING
}

#ifdef SENSCORD_SERVER_SETTING
/**
 * @brief Convert local_config to user server list.
 * @return Status object.
 */
Status ConfigurationCore::ConvertToUserServerList() {
  util::AutoLock lock(&mutex_);
  server_list_uid_.clear();

  uint16_t random_val = 0;
  for (std::vector<ServerSetting>::const_iterator
           itr = local_config_.server_list.begin(),
           end = local_config_.server_list.end();
       itr != end; ++itr) {
    // check list size
    if (server_list_uid_.size() >= kConfigServerMax) {
      SENSCORD_LOG_WARNING(
          "the number of server settings exceeds the upper limit");
      return Status::OK();
    }
    bool uid_generate = false;
    while (!uid_generate) {
      int32_t result = osal::OSRand(&random_val);
      if (result != 0) {
        return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseAborted,
            "failed to generate random value. (result=%" PRIx32 ")", result);
      }
      typedef std::map<uint32_t, ConfigArgument>::const_iterator Iter;
      Iter uid_itr = server_list_uid_.find(random_val);
      Iter uid_end = server_list_uid_.end();
      if (uid_itr != uid_end) {
        continue;
      }
      server_list_uid_[random_val] = itr->arguments;
      uid_generate = true;
    }
  }
  return Status::OK();
}

/**
 * @brief Convert user server list to local_config server list.
 */
void ConfigurationCore::ConvertToLocalConfigServerList() {
  util::AutoLock lock(&mutex_);
  local_config_.server_list.clear();

  for (std::map<uint32_t, ConfigArgument>::const_iterator
           itr = server_list_uid_.begin(),
           end = server_list_uid_.end();
       itr != end; ++itr) {
    ServerSetting tmp;
    tmp.arguments = itr->second;
    local_config_.server_list.push_back(tmp);
  }
}
#endif  // SENSCORD_SERVER_SETTING

/**
 * @brief Get configuration.
 */
const CoreConfig& ConfigurationCore::GetConfig() const {
  util::AutoLock lock(&mutex_);
  return local_config_;
}

/**
 * @brief Initialize configuration.
 * @return Status object.
 */
Status ConfigurationCore::InitConfig() {
  Status status;
  std::vector<std::string> env_paths;
  Environment::GetSensCordFilePath(&env_paths);
  if (env_paths.empty()) {
    // Get paths from environment variable.
    status = util::GetEnvironmentPaths(kSensCordFilePathEnvStr, &env_paths);
    if (status.ok() && !env_paths.empty()) {
      // Overwrite senscord file path from environment variable.
      status = Environment::SetSensCordFilePath(env_paths);
    }
  }

  if (status.ok()) {
    ConfigManager config_manager;
    std::string path;
    if (!util::SearchFileFromEnv(kSenscordConfigFile, &path)) {
      status = config_manager.SetDefaultConfig();
      SENSCORD_STATUS_TRACE(status);
    } else {
      status = config_manager.ReadConfig(path);
      SENSCORD_STATUS_TRACE(status);
    }
    if (status.ok()) {
      std::string identification = "";
      osal::OSGetEnvironment(kSensCordIdentification, &identification);
      status = config_manager.FinalizeConfig(identification);
      SENSCORD_STATUS_TRACE(status);
    }
    if (status.ok()) {
      local_config_ = *(config_manager.GetConfig());
    }
  }

  if (status.ok()) {
    std::string path;
    if (util::SearchFileFromEnv(kAllocatorConfigFile, &path)) {
      status = AllocatorConfigReader::ReadConfig(
          path, &local_config_.allocator_list);
      SENSCORD_STATUS_TRACE(status);
    }
  }

#ifdef SENSCORD_SERVER_SETTING
  if (status.ok()) {
    status = ConvertToUserServerList();
    SENSCORD_STATUS_TRACE(status);
  }
#endif  // SENSCORD_SERVER_SETTING

  return status;
}

}  // namespace senscord
