/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "c_api/c_core.h"
#include <inttypes.h>
#include <map>
#include <vector>
#include <algorithm>
#include <limits>   // numeric_limits
#include "senscord/c_api/senscord_c_api.h"
#include "senscord/senscord.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/serialize.h"
#include "c_api/c_common.h"
#include "c_api/c_stream.h"
#include "c_api/converter_manager.h"
#include "c_api/c_config_reader.h"
#include "stream/stream_core.h"
#include "util/autolock.h"
#include "util/senscord_utils.h"

namespace c_api = senscord::c_api;
namespace util = senscord::util;
namespace osal = senscord::osal;
namespace serialize = senscord::serialize;

namespace {

/**
 * @brief Copy to version(c) from version(cpp).
 * @param[in] src Version structure(cpp).
 * @param[out] dst Version structure(c).
 */
void CopyToVersionPropertyStructure(
    const senscord::VersionProperty& src, senscord_version_property_t* dst) {
  if (dst == NULL) {
    SENSCORD_LOG_ERROR("invalid parameter");
    return;
  }
  uint32_t size = sizeof(dst->name);
  c_api::StringToCharArray(src.name, dst->name, &size);
  dst->major = src.major;
  dst->minor = src.minor;
  dst->patch = src.patch;
  size = sizeof(dst->description);
  c_api::StringToCharArray(src.description, dst->description, &size);
}

#ifdef SENSCORD_STREAM_VERSION
/**
 * @brief Copy to stream version(c) from stream version(cpp).
 * @param[in] src StreamVersion structure(cpp).
 * @param[out] dst StreamVersion structure(c).
 * @return Status object.
 */
senscord::Status CopyToStreamVersionStructure(
    const senscord::StreamVersion& src, senscord_stream_version_t* dst) {
  SENSCORD_STATUS_ARGUMENT_CHECK(dst == NULL);
  CopyToVersionPropertyStructure(
      src.stream_version, &dst->stream_version);
  dst->destination_id = src.destination_id;
  dst->linkage_count = 0;
  dst->linkage_versions = NULL;
  if (src.linkage_versions.size() != 0) {
    uint32_t linkage_count = 0;
    if (src.linkage_versions.size() > std::numeric_limits<uint32_t>::max()) {
      linkage_count = std::numeric_limits<uint32_t>::max();
    } else {
      linkage_count = static_cast<uint32_t>(src.linkage_versions.size());
    }
    dst->linkage_count = linkage_count;
    dst->linkage_versions = new senscord_version_property_t[dst->linkage_count];
    std::vector<senscord::Version>::const_iterator itr =
        src.linkage_versions.begin();
    for (size_t index = 0; index < dst->linkage_count; ++index, ++itr) {
      CopyToVersionPropertyStructure(
          *itr, &dst->linkage_versions[index]);
    }
  }
  return senscord::Status::OK();
}
#endif  // SENSCORD_STREAM_VERSION

/**
 * @brief Copy to senscord version(c) from senscord version(cpp).
 * @param[in] src SensCordVersion structure(cpp).
 * @param[out] dst SensCordVersion structure(c).
 * @return Status object.
 */
senscord::Status CopyToSensCordVersionStructure(
    const senscord::SensCordVersion& src, senscord_version_t* dst) {
  SENSCORD_STATUS_ARGUMENT_CHECK(dst == NULL);
  CopyToVersionPropertyStructure(
      src.senscord_version, &dst->senscord_version);
  CopyToVersionPropertyStructure(
      src.project_version, &dst->project_version);
  dst->stream_count = 0;
  dst->stream_versions = NULL;
#ifdef SENSCORD_STREAM_VERSION
  if (src.stream_versions.size() != 0) {
    uint32_t stream_count = 0;
    if (src.stream_versions.size() > std::numeric_limits<uint32_t>::max()) {
      stream_count = std::numeric_limits<uint32_t>::max();
    } else {
      stream_count = static_cast<uint32_t>(src.stream_versions.size());
    }
    dst->stream_count = stream_count;
    dst->stream_versions = new senscord_stream_version_t[dst->stream_count];
    typedef std::map<std::string, senscord::StreamVersion> stream_map;
    stream_map::const_iterator itr = src.stream_versions.begin();
    for (size_t index = 0; index < dst->stream_count; ++index, ++itr) {
      senscord_stream_version_t* element = &dst->stream_versions[index];
      uint32_t size = sizeof(element->stream_key);
      c_api::StringToCharArray(itr->first, element->stream_key, &size);
      senscord::Status status =
          CopyToStreamVersionStructure(itr->second, element);
      if (!status.ok()) {
        return SENSCORD_STATUS_TRACE(status);
      }
    }
  }
#endif  // SENSCORD_STREAM_VERSION
  dst->server_count = 0;
  dst->server_versions = NULL;
#ifdef SENSCORD_STREAM_VERSION
  if (src.server_versions.size() != 0) {
    uint32_t server_count = 0;
    if (src.server_versions.size() > std::numeric_limits<uint32_t>::max()) {
      server_count = std::numeric_limits<uint32_t>::max();
    } else {
      server_count = static_cast<uint32_t>(src.server_versions.size());
    }
    dst->server_count = server_count;
    dst->server_versions = new senscord_version_t[dst->server_count];
    typedef std::map<int32_t, senscord::SensCordVersion> server_map;
    server_map::const_iterator sitr = src.server_versions.begin();
    for (size_t index = 0; index < dst->server_count; ++index, ++sitr) {
      senscord_version_t* element = &dst->server_versions[index];
      element->destination_id = sitr->first;
      senscord::Status status =
          CopyToSensCordVersionStructure(sitr->second, element);
      if (!status.ok()) {
        return SENSCORD_STATUS_TRACE(status);
      }
    }
  }
#endif  // SENSCORD_STREAM_VERSION
  return senscord::Status::OK();
}

/**
 * @brief Release senscord version data.
 * @param[in] version senscord version.
 */
void ReleaseSensCordVersion(struct senscord_version_t* version) {
#ifdef SENSCORD_STREAM_VERSION
  if (version == NULL) {
    SENSCORD_LOG_ERROR("invalid parameter");
    return;
  }
  for (uint32_t index = 0; index < version->stream_count; ++index) {
    senscord_stream_version_t* stream = &version->stream_versions[index];
    delete[] stream->linkage_versions;
  }
  delete[] version->stream_versions;
  for (uint32_t index = 0; index < version->server_count; ++index) {
    senscord_version_t* server = &version->server_versions[index];
    ReleaseSensCordVersion(server);
  }
  delete[] version->server_versions;
#endif  // SENSCORD_STREAM_VERSION
}

}  // namespace

/**
 * @brief Initialize Core, called at once.
 * @param[out] core  Core handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::Init
 */
int32_t senscord_core_init(
    senscord_core_t* core) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == NULL);
  senscord::Core* tmp_core = new senscord::Core;
  senscord::Status status = tmp_core->Init();
  if (!status.ok()) {
    delete tmp_core;
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }

  // read config
  std::string config_path;
  if (!senscord::util::SearchFileFromEnv(
      senscord::kSenscordConfigFile, &config_path)) {
    delete tmp_core;
    c_api::SetLastError(SENSCORD_STATUS_FAIL(
        senscord::kStatusBlockCore, senscord::Status::kCauseInvalidArgument,
        "%s not found.", senscord::kSenscordConfigFile));
    return -1;
  }
  std::vector<c_api::ConverterConfig> converters;
  status = senscord::c_api::ConfigReader::ReadConverterInfo(
      config_path, &converters);
  if (!status.ok()) {
    delete tmp_core;
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  senscord::ConverterManager::GetInstance()->Init(converters);

  c_api::CoreHandle* handle = new c_api::CoreHandle;
  handle->core = tmp_core;
  handle->senscord_version_cache = NULL;
  *core = c_api::ToHandle(handle);
  return 0;
}

/**
 * @brief Initialize Core with configuration.
 * @param[out] core    Core handle.
 * @param[in]  config  Config handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::Init
 */
int32_t senscord_core_init_with_config(
    senscord_core_t* core,
    senscord_config_t config) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(config == 0);
  c_api::ConfigHandle* config_handle =
      c_api::ToPointer<c_api::ConfigHandle*>(config);

  senscord::Core* tmp_core = new senscord::Core;
  senscord::Status status = tmp_core->Init(config_handle->config);
  if (!status.ok()) {
    delete tmp_core;
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }

  // read config
  std::string config_path;
  if (senscord::util::SearchFileFromEnv(
      senscord::kSenscordConfigFile, &config_path)) {
    std::vector<c_api::ConverterConfig> converters;
    status = senscord::c_api::ConfigReader::ReadConverterInfo(
        config_path, &converters);
    if (!status.ok()) {
      delete tmp_core;
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      return -1;
    }
    converters.insert(
        converters.end(),
        config_handle->converters.begin(), config_handle->converters.end());
    senscord::ConverterManager::GetInstance()->Init(converters);
  } else {
    senscord::ConverterManager::GetInstance()->Init(config_handle->converters);
  }

  c_api::CoreHandle* handle = new c_api::CoreHandle;
  handle->core = tmp_core;
  handle->senscord_version_cache = NULL;
  *core = c_api::ToHandle(handle);
  return 0;
}

/**
 * @brief Finalize Core and close all opened streams.
 * @param[in] core  Core handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::Exit
 */
int32_t senscord_core_exit(
    senscord_core_t core) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  senscord::Status status = handle->core->Exit();
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }

  senscord::ConverterManager::GetInstance()->Exit();

  {
    util::AutoLock _lock(&handle->mutex);
    if (handle->senscord_version_cache != NULL) {
      ReleaseSensCordVersion(handle->senscord_version_cache);
      delete handle->senscord_version_cache;
      handle->senscord_version_cache = NULL;
    }
  }
  delete handle->core;
  delete handle;
  return 0;
}

/**
 * @brief Get count of supported streams list.
 * @param[in]  core   Core handle.
 * @param[out] count  Count of supported streams list.
 * @return 0 is success or minus is failed (error code).
 * @see Core::GetStreamList
 */
int32_t senscord_core_get_stream_count(
    senscord_core_t core,
    uint32_t* count) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(count == NULL);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  {
    util::AutoLock _lock(&handle->mutex);
    if (handle->supported_stream_list_cache.empty()) {
      senscord::Status status =
          handle->core->GetStreamList(&handle->supported_stream_list_cache);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    *count = static_cast<uint32_t>(handle->supported_stream_list_cache.size());
  }
  return 0;
}

/**
 * @brief Get supported stream information.
 * @param[in]  core         Core handle.
 * @param[in]  index        Index of supported streams list.
 *                          (min=0, max=count-1)
 * @param[out] stream_info  Location of stream information.
 * @return 0 is success or minus is failed (error code).
 * @see Core::GetStreamList
 */
int32_t senscord_core_get_stream_info(
    senscord_core_t core,
    uint32_t index,
    struct senscord_stream_type_info_t* stream_info) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(stream_info == NULL);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  {
    util::AutoLock _lock(&handle->mutex);
    if (handle->supported_stream_list_cache.empty()) {
      senscord::Status status =
          handle->core->GetStreamList(&handle->supported_stream_list_cache);
      if (!status.ok()) {
        c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
        return -1;
      }
    }
    if (index >= handle->supported_stream_list_cache.size()) {
      c_api::SetLastError(SENSCORD_STATUS_FAIL(
          senscord::kStatusBlockCore, senscord::Status::kCauseOutOfRange,
          "index(%" PRIu32 ") is larger than list.size(%" PRIuS ")",
          index, handle->supported_stream_list_cache.size()));
      return -1;
    }
    const senscord::StreamTypeInfo& tmp_info =
        handle->supported_stream_list_cache[index];
    stream_info->key = tmp_info.key.c_str();
    stream_info->type = tmp_info.type.c_str();
    stream_info->id = tmp_info.id.c_str();
  }
  return 0;
}

/**
 * @brief Get supported stream information.
 * @param[in]  core       Core handle.
 * @param[in]  index      Index of supported streams list.
 *                        (min=0, max=count-1)
 * @param[in]  param      The type of parameter to get.
 * @param[out] buffer     Location to store the parameter string.
 * @param[in,out] length  [in] Buffer size.
 *                        [out] String length. (not including '\0')
 * @return 0 is success or minus is failed (error code).
 * @see Core::GetStreamList
 */
int32_t senscord_core_get_stream_info_string(
    senscord_core_t core,
    uint32_t index,
    enum senscord_stream_info_param_t param,
    char* buffer,
    uint32_t* length) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(
      param < SENSCORD_STREAM_INFO_STREAM_KEY ||
      param > SENSCORD_STREAM_INFO_IDENTIFICATION);
  SENSCORD_C_API_ARGUMENT_CHECK(length == NULL);
  struct senscord_stream_type_info_t stream_info = {};
  int32_t ret = senscord_core_get_stream_info(core, index, &stream_info);
  if (ret == 0) {
    std::string input;
    switch (param) {
      case SENSCORD_STREAM_INFO_STREAM_KEY:
        input = stream_info.key;
        break;
      case SENSCORD_STREAM_INFO_STREAM_TYPE:
        input = stream_info.type;
        break;
      case SENSCORD_STREAM_INFO_IDENTIFICATION:
        input = stream_info.id;
        break;
    }
    senscord::Status status = c_api::StringToCharArray(
        input, buffer, length);
    if (!status.ok()) {
      c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
      ret = -1;
    }
  }
  return ret;
}

/**
 * @brief Get count of opened stream.
 * @param[in]  core        Core handle.
 * @param[in]  stream_key  Stream key.
 * @param[out] count       Count of opened stream.
 * @return 0 is success or minus is failed (error code).
 * @see Core::GetOpenedStreamCount
 */
int32_t senscord_core_get_opened_stream_count(
    senscord_core_t core,
    const char* stream_key,
    uint32_t* count) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(stream_key == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(count == NULL);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  uint32_t opened_count = 0;
  senscord::Status status = handle->core->GetOpenedStreamCount(
      stream_key, &opened_count);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  *count = opened_count;
  return 0;
}

/**
 * @brief Get the version of this core library.
 * @param[in]  core     Core handle.
 * @param[out] version  The version of this core library.
 * @return 0 is success or minus is failed (error code).
 * @see Core::GetVersion
 */
int32_t senscord_core_get_version(
    senscord_core_t core,
    struct senscord_version_t* version) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(version == NULL);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  senscord::SensCordVersion tmp_version = {};
  senscord::Status status = handle->core->GetVersion(&tmp_version);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  // convert structure
  struct senscord_version_t* convert_version = new senscord_version_t;
  status = CopyToSensCordVersionStructure(tmp_version, convert_version);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    ReleaseSensCordVersion(convert_version);
    delete convert_version;
    return -1;
  }
  // self is destination none
  convert_version->destination_id = SENSCORD_DESTINATION_STREAM_NONE;
  // cache version
  {
    util::AutoLock _lock(&handle->mutex);
    if (handle->senscord_version_cache != NULL) {
      ReleaseSensCordVersion(handle->senscord_version_cache);
      delete handle->senscord_version_cache;
    }
    handle->senscord_version_cache = convert_version;
  }
  *version = *convert_version;
  return 0;
}

/**
 * @brief Open the new stream from key.
 * @param[in]  core        Core handle.
 * @param[in]  stream_key  The key of the stream to open.
 * @param[out] stream      The new stream handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::OpenStream
 */
int32_t senscord_core_open_stream(
    senscord_core_t core,
    const char* stream_key,
    senscord_stream_t* stream) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(stream_key == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(stream == NULL);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  senscord::Stream* stream_ptr = NULL;
  senscord::Status status = handle->core->OpenStream(stream_key, &stream_ptr);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  *stream = c_api::ToHandle(stream_ptr);
  return 0;
}

/**
 * @brief Open the new stream from key and specified config.
 * @param[in]  core        Core handle.
 * @param[in]  stream_key  The key of the stream to open.
 * @param[in]  setting     Config to open stream.
 * @param[out] stream      The new stream handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::OpenStream
 */
int32_t senscord_core_open_stream_with_setting(
    senscord_core_t core,
    const char* stream_key,
    const struct senscord_open_stream_setting_t* setting,
    senscord_stream_t* stream) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  SENSCORD_C_API_ARGUMENT_CHECK(stream_key == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(setting == NULL);
  SENSCORD_C_API_ARGUMENT_CHECK(stream == NULL);
  senscord::OpenStreamSetting tmp_setting = {};
  tmp_setting.frame_buffering.num = setting->frame_buffering.num;
  switch (setting->frame_buffering.buffering) {
    case SENSCORD_BUFFERING_USE_CONFIG:
      tmp_setting.frame_buffering.buffering = senscord::kBufferingUseConfig;
      break;
    case SENSCORD_BUFFERING_DEFAULT:
      tmp_setting.frame_buffering.buffering = senscord::kBufferingDefault;
      break;
    case SENSCORD_BUFFERING_OFF:
      tmp_setting.frame_buffering.buffering = senscord::kBufferingOff;
      break;
    case SENSCORD_BUFFERING_ON:
      tmp_setting.frame_buffering.buffering = senscord::kBufferingOn;
      break;
    default:
      tmp_setting.frame_buffering.buffering =
          static_cast<senscord::Buffering>(setting->frame_buffering.buffering);
      break;
  }
  switch (setting->frame_buffering.format) {
    case SENSCORD_BUFFERING_FORMAT_USE_CONFIG:
      tmp_setting.frame_buffering.format = senscord::kBufferingFormatUseConfig;
      break;
    case SENSCORD_BUFFERING_FORMAT_DEFAULT:
      tmp_setting.frame_buffering.format = senscord::kBufferingFormatDefault;
      break;
    case SENSCORD_BUFFERING_FORMAT_DISCARD:
      tmp_setting.frame_buffering.format = senscord::kBufferingFormatDiscard;
      break;
    case SENSCORD_BUFFERING_FORMAT_OVERWRITE:
      tmp_setting.frame_buffering.format = senscord::kBufferingFormatOverwrite;
      break;
    default:
      tmp_setting.frame_buffering.format =
          static_cast<senscord::BufferingFormat>(
              setting->frame_buffering.format);
      break;
  }
  uint32_t arguments_count =
      std::min(setting->arguments_count,
      static_cast<uint32_t>(SENSCORD_STREAM_ARGUMENT_LIST_MAX));
  for (uint32_t i = 0; i < arguments_count; ++i) {
    tmp_setting.arguments[setting->arguments[i].name] =
        setting->arguments[i].value;
  }
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  senscord::Stream* stream_ptr = NULL;
  senscord::Status status =
      handle->core->OpenStream(stream_key, tmp_setting, &stream_ptr);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  *stream = c_api::ToHandle(stream_ptr);
  return 0;
}

/**
 * @brief Close the opened stream.
 * @param[in] core    Core handle.
 * @param[in] stream  The opened stream handle.
 * @return 0 is success or minus is failed (error code).
 * @see Core::CloseStream
 */
int32_t senscord_core_close_stream(
    senscord_core_t core,
    senscord_stream_t stream) {
  SENSCORD_C_API_ARGUMENT_CHECK(core == 0);
  c_api::CoreHandle* handle = c_api::ToPointer<c_api::CoreHandle*>(core);
  senscord::StreamCore* stream_ptr =
      c_api::ToPointer<senscord::StreamCore*>(stream);
  senscord::Status status = handle->core->CloseStream(stream_ptr);
  if (!status.ok()) {
    c_api::SetLastError(SENSCORD_STATUS_TRACE(status));
    return -1;
  }
  return 0;
}

