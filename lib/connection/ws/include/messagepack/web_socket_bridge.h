/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_CONNECTION_WS_INCLUDE_MESSAGEPACK_WEB_SOCKET_BRIDGE_H_
#define LIB_CONNECTION_WS_INCLUDE_MESSAGEPACK_WEB_SOCKET_BRIDGE_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "senscord/senscord_types.h"
#include "senscord/connection_types.h"
#include "senscord/serialize.h"
#include "messagepack/message_pack_property_base.h"
#include "messagepack/message_pack_frame_base.h"

/* CORE command */
#define CORE_CMD_BASE           10
#define OPEN_CORE               (CORE_CMD_BASE + 0)
#define CLOSE_CORE              (CORE_CMD_BASE + 1)
#define GET_STREAM_LIST         (CORE_CMD_BASE + 2)
#define GET_STREAM_STATUS       (CORE_CMD_BASE + 3)
#define GET_VERSION             (CORE_CMD_BASE + 4)
#define OPEN_SECONDARY_CONNECT  (CORE_CMD_BASE + 5)

/* STREAM command */
#define STREAM_CMD_BASE   50
#define SEND_STREAM       (STREAM_CMD_BASE + 0)
#define OPEN_STREAM       (STREAM_CMD_BASE + 1)
#define CLOSE_STREAM      (STREAM_CMD_BASE + 2)
#define START_STREAM      (STREAM_CMD_BASE + 3)
#define STOP_STREAM       (STREAM_CMD_BASE + 4)
#define GET_PROPERTY      (STREAM_CMD_BASE + 5)
#define SET_PROPERTY      (STREAM_CMD_BASE + 6)
#define GET_PROPERTY_LIST (STREAM_CMD_BASE + 7)
#define GET_STATE         (STREAM_CMD_BASE + 8)
#define SEND_EVENT        (STREAM_CMD_BASE + 9)
#define REGISTER_EVENT    (STREAM_CMD_BASE + 10)
#define UNREGISTER_EVENT  (STREAM_CMD_BASE + 11)
#define LOCK_PROPERTY     (STREAM_CMD_BASE + 12)
#define UNLOCK_PROPERTY   (STREAM_CMD_BASE + 13)

/* getState */
#define STREAM_READY 1
#define STREAM_RUNNING 2

/* PLAYER API */
#define PLAYER_API_BASE 100
#define PLY_UPLOAD_AND_OPEN (PLAYER_API_BASE + 0)
#define PLY_OPEN (PLAYER_API_BASE + 1)
#define PLY_START (PLAYER_API_BASE + 2)
#define PLY_STOP (PLAYER_API_BASE + 3)
#define PLY_PAUSE (PLAYER_API_BASE + 4)
#define PLY_STEP_FORWARD_FRAME (PLAYER_API_BASE + 5)
#define PLY_STEP_BACK_FRAME (PLAYER_API_BASE + 6)
#define PLY_CLOSE (PLAYER_API_BASE + 7)
#define PLY_REMOVE (PLAYER_API_BASE + 8)

/* SendFrame raw mode */
enum RawMode {
  RAW_MODE_NONE = 0,  // no data
  RAW_MODE_BINARY,    // raw        : vector<uint8_t>
  RAW_MODE_REF,       // raw_ref    : msgpack::type::raw_ref
  RAW_MODE_MAPPED,    // mapped_raw : uint32_t offset, uint32_t size
};
SENSCORD_SERIALIZE_ADD_ENUM(RawMode)

/////// WebSocket bridge management structures ///////

/**
 * @brief Accepted job messages.
 */
struct JobMessage {
  std::string handle;
  std::string uniq_key;
  int32_t command;
  uint32_t index;
  std::string stream_key;
  std::string property_key;
};

/**
 * @brief Stream information in the Open.
 */
struct OpenStreamInfo {
  std::string stream_key;
  uint64_t stream_id;
};

/////// WebSocket bridge transfer structures for JavaScript ///////

/**
 * @brief Extended data of open stream request.
 */
struct OpenStreamRequest {
  bool use_shared_memory;
  uint32_t shared_memory_size;
  SENSCORD_SERIALIZE_DEFINE(use_shared_memory, shared_memory_size)
};

/**
 * @brief Channel data to JS.
 */
struct WSF_Channel {
  uint32_t id;
  std::string data_type;
  uint32_t time_stamp_s;
  uint32_t time_stamp_ns;
  uint32_t num_property;     // number of the properties
  std::map<std::string, std::vector<uint8_t> > map_property;
  RawMode raw_mode;
  std::vector<uint8_t> raw;
  msgpack::type::raw_ref raw_ref;
  uint32_t mapped_raw_offset;
  uint32_t mapped_raw_size;
  SENSCORD_SERIALIZE_DEFINE(
      id, data_type, time_stamp_s, time_stamp_ns, num_property, map_property,
      raw_mode, raw, raw_ref, mapped_raw_offset, mapped_raw_size)
};

/**
 * @brief Frame data to JS.
 */
struct FrameData {
  uint64_t SequenceNumber;  // javascript available 53bits
  uint32_t sequence_number_low;
  uint32_t sequence_number_high;
  std::string type;
  int32_t channel_num;
  std::vector<WSF_Channel> channel_list;
  SENSCORD_SERIALIZE_DEFINE(
      SequenceNumber, sequence_number_low, sequence_number_high, type,
      channel_num, channel_list)
};

/**
 * @brief Multiple Frames were combined.
 */
struct Frames {
  std::vector<FrameData> frames;
  SENSCORD_SERIALIZE_DEFINE(frames)
};

/**
 * @brief Standard request data from JS.
 */
struct RequestMessage {
  std::string handle;
  std::string uniq_key;
  int32_t command;
  uint32_t index;
  std::string property_key;
  std::string stream_key;
  std::string primary_handle;
  uint32_t msg_pack_data_exist;
  std::vector<uint8_t> msg_pack_data;

  // for REGISTER_EVENT, UNREGISTER_EVENT
  std::string event_type;

  // for LOCK_PROPERTY
  std::set<std::string> keys;
  int32_t timeout_msec;

  // for UNLOCK_PROPERTY
  std::string resource;

  SENSCORD_SERIALIZE_DEFINE(
      handle, uniq_key, command, index, property_key, stream_key,
      primary_handle, msg_pack_data_exist, msg_pack_data, event_type,
      keys, timeout_msec, resource)
};

/**
 * @brief Standard response data to JS.
 */
class ResponseMessage {
 public:
  std::string handle;
  std::string uniq_key;
  int32_t command;
  uint32_t index;
  bool result;
  senscord::MessageStatus status;
  SENSCORD_SERIALIZE_DEFINE(handle, uniq_key, command, index, result, status)

  explicit ResponseMessage(const JobMessage &jobMessage) {
    uniq_key = jobMessage.uniq_key;
    handle = jobMessage.handle;
    command = jobMessage.command;
    index = jobMessage.index;
    result = false;
  }
};

/**
 * @brief Extended response data to JS.
 */
template <typename T>
class ResponseDataMessage {
 public:
  std::string handle;
  std::string uniq_key;
  int32_t command;
  uint32_t index;
  bool result;
  senscord::MessageStatus status;
  T data;
  SENSCORD_SERIALIZE_DEFINE(
      handle, uniq_key, command, index, result, status, data)

  ResponseDataMessage() { result = false; }
  explicit ResponseDataMessage(const JobMessage &jobMessage) {
    uniq_key = jobMessage.uniq_key;
    handle = jobMessage.handle;
    command = jobMessage.command;
    index = jobMessage.index;
    result = false;
  }
};

/**
 * @brief GetStreamList reply data for JS.
 */
struct StreamInfoDataReply {
  int32_t num;
  std::vector<std::string> keyList;
  std::vector<std::string> typeList;
  std::vector<std::string> idList;

  SENSCORD_SERIALIZE_DEFINE(num, keyList, typeList, idList)
};

/**
 * @brief OpenStream reply data to JS.
 */
struct OpenStreamReply {
  std::string shared_memory_name;
  uint32_t shared_memory_size;

  SENSCORD_SERIALIZE_DEFINE(shared_memory_name, shared_memory_size)
};

/**
 * @brief GetPropertyList reply data to JS.
 */
struct PropertyListDataReply {
  std::vector<std::string> property_list;

  SENSCORD_SERIALIZE_DEFINE(property_list)
};

/**
 * @brief LockProperty reply data to JS.
 */
struct LockPropertyReply {
  std::string resource_id;

  SENSCORD_SERIALIZE_DEFINE(resource_id)
};

namespace senscord {

/**
 * @brief The Messages Pack/Unpack class for WebSocket.
 */
class WebSocketBridge {
 public:
  WebSocketBridge();
  ~WebSocketBridge();

  /**
   * @brief Received Property MessagePack to SensCord BinaryProperty.
   * @param[in] (key) Key name of the property.
   * @param[in] (src) Message pack body of the property.
   * @param[in/out] (dst) Key of the property.
   * @return Status object.
   */
  Status PropertyPackToBinary(const std::string key,
      std::vector<uint8_t>* src, std::vector<uint8_t>* dst);

  /**
   * @brief SensCord BinaryProperty to Property MessagePack for send data.
   * @param[in] (key) Key name of the property.
   * @param[in] (src) Body of the property.
   * @param[in/out] (dst) Messagepack body of the property.
   * @return Status object.
   */
  Status BinaryToPropertyPack(const std::string key,
      std::vector<uint8_t>* src, std::vector<uint8_t>* dst);

  /**
   * @brief Serialize frame to MessagePack for send data.
   * @param[in] (stream_key) Key name of the stream.
   * @param[in] (handle) The JS handle for sending SendFrame.
   * @param[in] (data) The message data for sending SendFrame.
   * @param[in] (jobMessage) Accepted job messages.
   * @param[in/out] (dst) Messagepack body of the property.
   * @return Status object.
   */
  Status SerializeFrameToMsgPack(
      const std::string stream_key,
      const std::string handle,
      const MessageDataFrameLocalMemory &data,
      const JobMessage &jobMessage,
      std::vector<uint8_t> &dst);

  /**
   * @brief Register to property component list.
   */
  template <typename T>
  void regist() {
    MessagePackPropertyBase* component = new T();
    property_comp_list_.insert(PropertyCompList::value_type(
        component->GetInstanceName(), component));
  }

 private:
  // Property MessagePack component instance mappings
  typedef std::map<std::string, MessagePackPropertyBase *> PropertyCompList;
  PropertyCompList property_comp_list_;

  // Frame MessagePack component instance mappings
  // comment out for future use.
  // typedef std::map<std::string, MessagePackFrameBase *> FrameCompList;
  // FrameCompList frame_comp_list_;
};

}  // namespace senscord

#endif  // LIB_CONNECTION_WS_INCLUDE_MESSAGEPACK_WEB_SOCKET_BRIDGE_H_
