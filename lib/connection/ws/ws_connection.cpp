/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/types.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>

#include "senscord/osal_inttypes.h"
#include "senscord/osal.h"
#include "senscord/logger.h"
#include "senscord/serialize.h"
#include "senscord/memory_allocator.h"
#include "ws_connection.h"
#include "ws_connection_manager.h"
#include "ws_log_macro.h"

// When enabling the shared memory function, enable the following definitions.
// #define USE_SHARED_MEMORY_FOR_RAWDATA

#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
#include "shared_memory_manager.h"
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA

#define BUF_LEN 2048
#define BUF_MAX_HANDSHAKE 65536

// export register function.
SENSCORD_REGISTER_CONNECTION(senscord::WsConnection)

namespace senscord {

// sync word for messgae header.
static const uint32_t kMessageSyncWord = 0xDEADC0DE;

// backlog size for listen.
static const int32_t kBacklogSize = 3;

#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
// Default shared memory size.
const uint32_t kDefaultSharedMemorySize = 0x2000000;  // 32MB
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA

/**
 * @brief The worker method for threading.
 * @param[in] (arg) The instance of connection class.
 * @return Don't care.
 */
static osal::OSThreadResult Thread(void* arg) {
  if (arg) {
    WsConnection* thread = reinterpret_cast< WsConnection*>(arg);
    thread->Monitoring();
  }
  return static_cast<osal::OSThreadResult>(0);
}

/**
 * @brief Start to socket accept control.
 * @return Status object.
 */
int32_t WsConnection::Start() {
  if (thread_) {
    return -1;
  }
  end_flag_ = false;
  osal::OSCreateThread(&thread_, Thread, this, NULL);
  return 0;
}

/**
 * @brief Stop to socket accept control.
 * @return Status object.
 */
int32_t WsConnection::Stop() {
  LOG_D("enter thread=0x%x", thread_);
  if (thread_) {
    end_flag_ = true;
    SetEvent(cond_receive_job_);
    SetEvent(cond_recvive_finish_);
    osal::OSJoinThread(thread_, NULL);
    thread_ = NULL;
  }
  LOG_D("leave thread=0x%x", thread_);
  return 0;
}

/**
 * @brief The method of the socket accept control thread.
 */
void WsConnection::Monitoring() {
  LOG_D("start socket=0x%x", socket_);

  while (!end_flag_) {
    if (socket_ == NULL || !listening_flag_) {
      osal::OSSleep(10 * 1000 * 1000);
    } else {
      {
        AutoMutex auto_mutex(mutex_receive_job_);
        if (data_available_) {
          WaitEvent(mutex_receive_job_, cond_recvive_finish_, 10 * 1000 * 1000);
          continue;
        }
      }

      std::vector<osal::OSSocket*> readable;
      readable.push_back(socket_);
      int32_t ret = osal::OSRelativeTimedSelectSocket(
          &readable, NULL, NULL, 10 * 1000 * 1000);
      if (ret >= 0) {
        AutoMutex auto_mutex_release(mutex_receive_job_);
        struct ReceiveJob job = { ReceiveJobType::RECEIVE_JOB_SOCKET, NULL };
        receive_job_.push_back(job);
        data_available_ = true;
        SetEvent(cond_receive_job_);
      }
    }
  }
  LOG_D("end socket=0x%x", socket_);
}

/**
 * @brief Send the message to the connected target witch WebSocket.
 * @param[in] (data) The sending data.
 * @param[in] (len) The sending data length.
 * @return value of the sendmsg.
 */
int32_t WsConnection::Send(const void* data, size_t len) {
  int32_t ret = 0;

  // send message
  if (socket_) {
    class AutoMutex auto_mutex(mutex_);
    size_t sent_size = 0;
    ret = osal::OSSendSocket(socket_, data, len, &sent_size);
    if (ret < 0) {
      LOG_E("send error=%x", ret);
    }
  }
  return ret;
}

/**
 * @brief Send the encode message to the connected target witch WebSocket.
 * @param[in] (vec) The sending vector data.
 * @param[in] (veclen) The sending vector data length.
 * @return value of the sendmsg.
 */
int32_t WsConnection::Send(const iovec* vec , size_t veclen) {
  std::vector<osal::OSSocketMessage> messages;
  osal::OSSocketMessage msg = {};
  size_t len = 0;
  for (size_t i = 0; i < veclen; ++i) {
    msg.buffer = vec[i].iov_base;
    msg.buffer_size = vec[i].iov_len;
    messages.push_back(msg);
    len += vec[i].iov_len;
  }

  uint8_t outFrame[10] {};
  size_t outLength = CreatePayloadLength(
      WS_BINARY_FRAME, len, outFrame);
  if (outLength == 0) {
    return -1;
  }

  msg.buffer = const_cast<char*>(reinterpret_cast<char*>(&outFrame));
  msg.buffer_size = outLength;
  messages.insert(messages.begin(), msg);

  size_t sent_size = 0;
  int32_t ret = 0;
  if (socket_) {
    class AutoMutex auto_mutex(mutex_);
    ret = osal::OSSendMsgSocket(socket_, messages, NULL, &sent_size);
    if (ret < 0) {
      LOG_E("send error=%x", ret);
    }
  }
  return ret;
}

/**
 * @brief Send the encode message to the connected target witch WebSocket.
 * @param[in] (data) The sending data.
 * @param[in] (len) The sending data length.
 * @param[in] (frameType) Frame type.
 * @return value of the sendmsg.
 */
int32_t WsConnection::Send(
    const void* data, size_t len, enum wsFrameType frameType) {
  uint8_t outFrame[10] {};
  size_t outLength = CreatePayloadLength(frameType, len, outFrame);
  if (outLength == 0) {
    return -1;
  }

  std::vector<osal::OSSocketMessage> messages;
  osal::OSSocketMessage msg = {};

  msg.buffer = const_cast<char*>(reinterpret_cast<char*>(&outFrame));
  msg.buffer_size = outLength;
  messages.push_back(msg);
  if (len != 0) {
    msg.buffer = const_cast<char*>(reinterpret_cast<const char*>(data));
    msg.buffer_size = len;
    messages.push_back(msg);
  }
  size_t sent_size = 0;
  int32_t ret = 0;
  if (socket_) {
    class AutoMutex auto_mutex(mutex_);
    ret = osal::OSSendMsgSocket(socket_, messages, NULL, &sent_size);
    if (ret < 0) {
      LOG_E("send error=%x", ret);
    }
  }
  return ret;
}

/**
 * @brief read WebSocket message.
 * @param[in] (param) The parameter of payload buffer which loaded.
 * @return Frame type(see enum wsFrameType).
 */
enum wsFrameType WsConnection::readWsMessage(
    std::vector<uint8_t>& payload, int * length) {
  *length = 0;
  size_t received_size = 0;

  std::vector<uint8_t> data(BUF_LEN);
  size_t buffer_size = data.size();
  enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
  uint8_t *payload_data = NULL;
  size_t payload_size = 0;
  size_t next_packet_len = 0;

  while (frameType == WS_INCOMPLETE_FRAME) {
    size_t size = 0;
    if (buffer_size == received_size) {
      if ((state_ == WS_STATE_OPENING) &&
          ((buffer_size + BUF_LEN) > BUF_MAX_HANDSHAKE)) {
        frameType = WS_ERROR_FRAME;
        break;
      }
      data.resize(buffer_size + BUF_LEN);
      buffer_size = data.size();
    }
    LOG_D("buffer size=%" PRIuS, buffer_size);
    if (!next_packet_.empty()) {
      data.swap(next_packet_);
      next_packet_.clear();
      size = data.size();
      LOG_D("next data get : size=%" PRIuS, size);
    } else {
      // receive one packet.
      int32_t ret = osal::OSRecvSocket(socket_, &(data[received_size]),
          buffer_size - received_size, &size);
      if (ret < 0) {
        // error
        LOG_E("closing a socket. socket=0x%x, error=%x", socket_, ret);
        return WS_CLOSING_FRAME;
      } else if (size == 0) {
        // disconnected
        LOG_D("disconnected detected. socket=0x%x", socket_);
        frameType = WS_CLOSING_FRAME;
        break;
      }
    }
    LOG_D("read size=%" PRIuS, size);
    received_size += size;
    if (state_ == WS_STATE_OPENING) {
      if (received_size >= 4) {
        if (osal::OSMemcmp(data.data(), "GET ", 4) == 0) {
          frameType = wsParseHandshake(data.data(), received_size, &hs_);
        } else {
          frameType = WS_ERROR_FRAME;
        }
      }
    } else {
      frameType = wsParseInputFrame(
          data.data(), received_size, &payload_data,
          &payload_size, &next_packet_len);
      if (frameType == WS_BINARY_FRAME && next_packet_len) {
        next_packet_.reserve(next_packet_len);
        next_packet_.assign(
            payload_data + payload_size,
            payload_data + payload_size + next_packet_len);
        LOG_D("next_packet_len=%" PRIuS, next_packet_len);
      }
    }
    LOG_D("frameType=%d, received_size=%" PRIuS, frameType, received_size);
  }  // end of while
  LOG_D("frameType=%d, total size=%" PRIuS, frameType, received_size);

  if (frameType == WS_ERROR_FRAME) {
    if (state_ == WS_STATE_OPENING) {
      LOG_W("received error frame, opening error");
      char * buff = new char[BUF_LEN];
      int32_t size = 0;
#ifdef _WIN32
      size = sprintf_s(buff, BUF_LEN,
#else
      size = snprintf(buff, BUF_LEN,
#endif
          "HTTP/1.1 400 Bad Request\r\n"
          "%s%s\r\n\r\n",
          versionField,
          version);
      if (size < 0) {
        LOG_E("store string error=%x", size);
      } else {
        if (Send(buff, size) < 0) {
          frameType = WS_ERROR_FRAME;
        }
      }
      delete[] buff;
    } else {
      LOG_W("receved error frame, closeing");
      frameType = WS_CLOSING_FRAME;
      state_ = WS_STATE_CLOSING;
      if (Send(NULL, 0, frameType) < 0) {
        frameType = WS_ERROR_FRAME;
      }
    }
  } else if (frameType == WS_OPENING_FRAME) {
    if (state_ == WS_STATE_OPENING) {
      char * buff = new char[BUF_LEN];
      size_t frameSize = BUF_LEN;
      // if resource is right, generate answer handshake and send it
      wsGetHandshakeAnswer(&hs_, reinterpret_cast<uint8_t*>(buff), &frameSize);
      LOG_D("Send Frame=");
      if (frameSize == 0) {
        LOG_E("generate answer handshake error.");
      } else {
        if (Send(buff, frameSize) == 0) {
          state_ = WS_STATE_NORMAL;
        }
      }
      freeHandshake(&hs_);
      delete[] buff;
    }
  } else if (frameType == WS_CLOSING_FRAME) {
    LOG_D("Recieved Closing");
    Close();
  } else if (frameType == WS_BINARY_FRAME) {
    payload.assign(payload_data, payload_data + payload_size);
    *length = static_cast<int>(payload_size);
    LOG_D("Recieved length=%d", *length);
  }
  return frameType;
}


/**
 * @brief Open the connection.
 * @return Status object.
 */
Status WsConnection::Open() {
  if (socket_) {
    LOG_E("already opened");
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "already opened");
  }

  int32_t ret = osal::OSCreateSocket(osal::kSocketTypeInetTcp, &socket_);
  if (ret < 0) {
    LOG_E("error=%x", ret);
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "failed to create socket");
  }
  LOG_D("Open(): socket=0x%x", socket_);
  return Status::OK();
}

/**
 * @brief Close the connection.
 * @return Status object.
 */
Status WsConnection::Close() {
  LOG_D("Close(): --> enter socket=0x%x", socket_);
  Status status = Status::OK();
  if (socket_) {
    listening_flag_ = false;
    AutoMutex auto_mutex(mutex_);
    LOG_D("Close(): ShutdownSocket call");
    osal::OSSocket* socket = socket_;
    socket_ = NULL;
    // force shutdown
    int32_t ret = osal::OSShutdownSocket(socket, osal::kShutdownBoth);
    if (ret < 0) {
      LOG_E("shutdown socket error=%x", ret);
    }
    LOG_D("Close(): DestroySocket call ");
    // close
    ret = osal::OSDestroySocket(socket);
    LOG_D("Close(): socket=0x%x, ret=%x",
        socket_, ret);
    if (ret < 0) {
      status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation,
          "failed to destroy socket: %" PRIx32,
          ret);
    }
    osal::OSSleep(500000000);
  }

  LOG_D("Close(): job clean");
  {
    AutoMutex auto_mutex_job(mutex_job_);
    JobList::iterator itr1 = jobs_.begin();
    JobList::iterator end1 = jobs_.end();
    for (; itr1 != end1; ++itr1) {
      JobMessage* jobMsg = itr1->second;
      delete jobMsg;
    }
    jobs_.clear();
  }
  LOG_D("Close(): handle clean");
  {
    AutoMutex auto_mutex_handle(mutex_handle_);
    HandleStreamList::const_iterator itr2 = handle_stream_.begin();
    HandleStreamList::const_iterator end2 = handle_stream_.end();
    for (; itr2 != end2; ++itr2) {
      const OpenStreamInfo* info = &itr2->second;
      auto manager = ws::WsConnectionManager::GetInstance();
      manager->UnregisterConnection(info->stream_id, this);
      manager->UnregisterHandle(itr2->first);
#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
      // Close shared memory.
      auto shm_manager = ws::SharedMemoryManager::GetInstance();
      if (shm_manager->IsSharedMemory(info->stream_id)) {
        ws::ConnectionInfo tmp = {};
        Status ret = manager->GetConnection(info->stream_id, &tmp);
        if (!ret.ok()) {
          // Closes when connection information cannot be acquired.
          ret = shm_manager->Close(info->stream_id);
          if (!ret.ok()) {
            // error log
            LOG_E("Failed to close shared memory: %s",
                ret.ToString().c_str());
          }
        }
      }
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA
    }
    handle_stream_.clear();
  }
  LOG_D("Close(): release frame clean");
  {
    AutoMutex auto_mutex_release(mutex_receive_job_);
    data_available_ = false;
    auto itr4 = receive_job_.begin();
    while (itr4 != receive_job_.end()) {
      auto job = *itr4;
      if (job.type == ReceiveJobType::RECEIVE_JOB_REPLY) {
        Message * msg = reinterpret_cast<Message *>(job.address);
        ReleaseMessage(msg->header, msg->data);
        delete msg;
      }
      itr4++;
    }
    receive_job_.clear();
  }
  LOG_D("Close(): <-- leave");
  return status;
}

/**
 * @brief Bind as the server.
 * @param[in] (param) The informations of the binding.
 * @return Status object.
 */
Status WsConnection::Bind(const std::string& param) {
  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation, "not opened yet");
  }

  osal::OSSocketAddressInet addr = GetAddress(param);
  LOG_D("bind addr: 0x%" PRIx32 ":%" PRIu32,
      addr.address, addr.port);
  int32_t ret = osal::OSSetSocketReuseAddr(socket_, true);
  if (ret < 0) {
    LOG_E("failed to reuse bind address error=%x", ret);
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "failed to reuse bind address: 0x%" PRIx32, ret);
  }
  ret = osal::OSBindSocket(socket_, addr);
  if (ret < 0) {
    LOG_E("failed to bind error=%x", ret);
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "failed to bind: 0x%" PRIx32, ret);
  }
  address_ = addr;
  return Status::OK();
}

/**
 * @brief Start to listen the connection.
 * @return Status object.
 */
Status WsConnection::Listen() {
  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  int32_t ret = osal::OSListenSocket(socket_, kBacklogSize);
  if (ret < 0) {
    LOG_E("failed to listen error=%x", ret);
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to listen: 0x%" PRIx32, ret);
  }
  listening_flag_ = true;
  return Status::OK();
}

/**
 * @brief Accept the incoming connection.
 * @param[out] (new_connection) The new connection.
 * @param[out] (is_same_system) Whether new connection is on same system.
 * @return Status object.
 */
Status WsConnection::Accept(
    Connection** new_connection, bool* is_same_system) {
  runOnDestructor runOn([&]() {
    AutoMutex auto_mutex(mutex_receive_job_);
    data_available_ = false;
    receive_job_.erase(
        std::remove_if(receive_job_.begin(), receive_job_.end(),
          [](const struct ReceiveJob & job) {
            return job.type == ReceiveJobType::RECEIVE_JOB_SOCKET;
    }));
    SetEvent(cond_recvive_finish_);
  });

  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation, "not opened yet");
  }
  if (new_connection == NULL) {
    LOG_E("new_connection is null");
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidArgument,
        "new_connection is null");
  }

  osal::OSSocket* new_sock = NULL;
  osal::OSSocketAddressInet address = {};
  int32_t ret = osal::OSAcceptSocket(socket_, &new_sock, &address);
  if (ret < 0) {
    LOG_E("failed to accept error=%x", ret);
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "failed to accept: 0x%" PRIx32, ret);
  }

  // create new connection
  WsConnection* ws = new WsConnection(new_sock, address);
  *new_connection = ws;

  if (is_same_system) {
    *is_same_system = false;
  }
  LOG_D("accept connection=0x%x new socket=0x%x", ws, ws->socket_);
  return Status::OK();
}

/**
 * @brief Send the message to the connected target.
 * @param[in] (msg) The sending message.
 * @return Status object.
 */
Status WsConnection::Send(const Message& msg) {
  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }
  // store pack data for send.
  std::vector<uint8_t> vect;
  if (msg.header.type == kMessageTypeSendEvent) {
    return SendEvent(msg);
  }
  if (msg.header.type == kMessageTypeSendFrame) {
    return SendFrame(msg);
  }
  if (msg.header.type == kMessageTypeReply) {
    if (msg.header.data_type == kMessageDataTypeReleaseFrame) {
      return Status::OK();
    }
    JobMessage job_message;
    {
      JobList::const_iterator itr;
      class AutoMutex mutex_job(mutex_job_);
      itr = jobs_.find(msg.header.request_id);
      if (itr == jobs_.end()) {
        return SENSCORD_STATUS_FAIL(
            kStatusBlockCore, Status::kCauseUnknown,
            "request command not found");
      }
      JobMessage* jobMsg = itr->second;
      job_message = *jobMsg;
      jobs_.erase(itr);
      delete jobMsg;
    }

    LOG_D("send command=%d", job_message.command);
    if (msg.data == NULL) {
      ResponseMessage resMsg(job_message);
      resMsg.status.Set(SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseNotSupported,
          "unsupported message: type=%d, data_type=%d",
          msg.header.type, msg.header.data_type));
      resMsg.result = resMsg.status.ok;
      SerializeMsg(&resMsg, &vect);
      LOG_D("%s", resMsg.status.Get().ToString().c_str());
    } else {
      switch (msg.header.data_type) {
      case kMessageDataTypeStart:
      case kMessageDataTypeStop:
      case kMessageDataTypeClose:
      case kMessageDataTypeSetProperty:
      case kMessageDataTypeSecondaryConnect:
      case kMessageDataTypeRegisterEvent:
      case kMessageDataTypeUnregisterEvent:
      case kMessageDataTypeUnlockProperty:
        MakeStandardReplyMsg(msg, job_message, &vect);
        break;
      case  kMessageDataTypeOpen:
        MakeOpenStreamReplyMsg(msg, job_message, &vect);
        break;
      case kMessageDataTypeGetStreamList:
        MakeGetStreamListReplyMsg(msg, job_message, &vect);
        break;
      case kMessageDataTypeGetPropertyList:
        MakeGetPropertyListReplyMsg(msg, job_message, &vect);
        break;
      case kMessageDataTypeGetVersion:
        MakeGetVersionReplyMsg(msg, job_message, &vect);
        break;
      case kMessageDataTypeGetProperty:
        GetPropertyToMsgPack(msg, vect, job_message);
        break;
      case kMessageDataTypeLockProperty:
        MakeLockPropertyReplyMsg(msg, job_message, &vect);
        break;
      default:
        // do nothing
        break;
      }
    }
  }

  // send message
  if (vect.size() > 0) {
    int32_t ret = Send(vect.data(), vect.size(), WS_BINARY_FRAME);
    if (ret < 0) {
      LOG_E("send error =%x", ret);
    }
    // for debug
    LOG_D("send msg: size=%" PRIdS, vect.size());
  }
  return Status::OK();
}

/**
 * @brief Make a standard reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeStandardReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) {
  ResponseMessage resMsg(job_message);
  MessageDataStandardReply *reply_data =
      reinterpret_cast<MessageDataStandardReply*>(msg.data);
  resMsg.result = reply_data->status.ok;
  resMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    if (msg.header.data_type == kMessageDataTypeClose) {
      HandleStreamList::iterator itr;
      class AutoMutex mutex_handle(mutex_handle_);
      itr = handle_stream_.find(resMsg.handle);
      if (itr != handle_stream_.end()) {
        LOG_D("kMessageDataTypeClose:%s, id:%" PRId64
            ", cause=%d, Message=%s",
            resMsg.handle.c_str(),
            msg.header.server_stream_id,
            reply_data->status.cause,
            reply_data->status.message.c_str());
        OpenStreamInfo* info = &itr->second;
        // FIXME: unreachable code! (Shared memory is closed in WsConnection::Close function)
        /*
        // Close shared memory.
        auto shm_manager = ws::SharedMemoryManager::GetInstance();
        if (shm_manager->IsSharedMemory(info->stream_id)) {
          Status status = shm_manager->Close(info->stream_id);
          if (!status.ok()) {
            // error log
            LOG_E("Failed to close shared memory: %s", status.ToString().c_str());
          }
        }
        */
        auto manager = ws::WsConnectionManager::GetInstance();
        manager->UnregisterConnection(info->stream_id, this);
        manager->UnregisterHandle(itr->first);
        handle_stream_.erase(itr);
        resMsg.result = false;
      } else {
        resMsg.result = true;
      }
    } else if (msg.header.data_type == kMessageDataTypeSecondaryConnect) {
      OpenStreamInfo info = {};
      info.stream_id = msg.header.server_stream_id;
      info.stream_key =
          "secondary-key-" + std::to_string(msg.header.server_stream_id);
      insert(mutex_handle_, handle_stream_, resMsg.handle, info);
      // register the secondary connection.
      auto manager = ws::WsConnectionManager::GetInstance();
      manager->RegisterHandle(resMsg.handle, info.stream_id);
      manager->RegisterSecondaryConnection(info.stream_id, this);
      AutoMutex lock(mutex_secondary_);
      if (primary_ != nullptr) {
        LOG_I("SecondaryConnect: stream_id=%" PRIx64
            ", primary=%p, secondary=%p",
            info.stream_id, primary_, this);
      }
    }
  }
  LOG_D("[reply] handle=%s, data_type=%d, message=%s",
      resMsg.handle.c_str(), msg.header.data_type,
      reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resMsg, buffer);
}

/**
 * @brief Make a OpenStream reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeOpenStreamReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) {
  ResponseDataMessage<OpenStreamReply> resMsg(job_message);
  const MessageDataOpenReply* reply_data =
      reinterpret_cast<MessageDataOpenReply*>(msg.data);
  resMsg.result = reply_data->status.ok;
  resMsg.status = reply_data->status;
  resMsg.data.shared_memory_name.clear();
  resMsg.data.shared_memory_size = 0;
  if (reply_data->status.ok) {
    OpenStreamInfo info = {};
    info.stream_id = msg.header.server_stream_id;
    info.stream_key = job_message.stream_key;
    insert(mutex_handle_, handle_stream_, resMsg.handle, info);
    // register the primary connection.
    auto manager = ws::WsConnectionManager::GetInstance();
    manager->RegisterHandle(resMsg.handle, info.stream_id);
    manager->RegisterPrimaryConnection(info.stream_id, this);
#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
    // Get shared memory size.
    auto shm_manager = ws::SharedMemoryManager::GetInstance();
    uint32_t memory_size = shm_manager->GetSharedMemorySize(info.stream_key);
    if (memory_size != 0) {
      // Open shared memory.
      std::string memory_name;
      Status status = shm_manager->Open(
          info.stream_id, memory_size, &memory_name);
      if (status.ok()) {
        ws::SharedMemoryParameter param = {};
        shm_manager->GetMemoryParameter(info.stream_id, &param);
        resMsg.data.shared_memory_name = memory_name;
        resMsg.data.shared_memory_size =
            static_cast<uint32_t>(param.total_size);
      } else {
        // error log
        LOG_E("Failed to open shared memory: %s", status.ToString().c_str());
      }
    }
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA
  }
  LOG_D("[reply] OpenStream: handle=%s, stream_id=%" PRId64 ", status=%s",
      resMsg.handle.c_str(), msg.header.server_stream_id,
      reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resMsg, buffer);
}

/**
 * @brief Make a GetStreamList reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeGetStreamListReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) const {
  ResponseDataMessage<StreamInfoDataReply> resDataMsg(job_message);
  const MessageDataStreamListReply* reply_data =
      static_cast<MessageDataStreamListReply*>(msg.data);
  resDataMsg.result = reply_data->status.ok;
  resDataMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    resDataMsg.data.num =
        static_cast<int32_t>(reply_data->stream_list.size());
    resDataMsg.data.keyList.resize(resDataMsg.data.num);
    resDataMsg.data.typeList.resize(resDataMsg.data.num);
    resDataMsg.data.idList.resize(resDataMsg.data.num);
    for (int32_t i = 0; i < resDataMsg.data.num; ++i) {
      resDataMsg.data.keyList[i] = reply_data->stream_list[i].key;
      resDataMsg.data.typeList[i] = reply_data->stream_list[i].type;
      resDataMsg.data.idList[i] = reply_data->stream_list[i].id;
    }
  }
  LOG_D("[reply] GetStreamList: handle=%s, message=%s",
      resDataMsg.handle.c_str(), reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resDataMsg, buffer);
}

/**
 * @brief Make a GetPropertyList reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeGetPropertyListReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) const {
  ResponseDataMessage<PropertyListDataReply> resDataMsg(job_message);
  const MessageDataPropertyListReply* reply_data =
      reinterpret_cast<MessageDataPropertyListReply*>(msg.data);
  resDataMsg.result = reply_data->status.ok;
  resDataMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    resDataMsg.data.property_list = reply_data->property_list;
  }
  LOG_D("[reply] GetPropertyList: handle=%s, message=%s",
      resDataMsg.handle.c_str(), reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resDataMsg, buffer);
}

/**
 * @brief Make a GetVersion reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeGetVersionReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) const {
  ResponseDataMessage<MessageDataVersionReply> resDataMsg(job_message);
  const MessageDataVersionReply* reply_data =
      reinterpret_cast<MessageDataVersionReply*>(msg.data);
  resDataMsg.result = reply_data->status.ok;
  resDataMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    resDataMsg.data = *reply_data;
  }
  LOG_D("[reply] GetVersion: handle=%s, message=%s",
      resDataMsg.handle.c_str(), reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resDataMsg, buffer);
}

/**
 * @brief Make a LockProperty reply message.
 * @param[in] (msg) The sending message.
 * @param[in] (job_message) The job message.
 * @param[out] (vect) Message packed vector.
 */
void WsConnection::MakeLockPropertyReplyMsg(
    const Message& msg, const JobMessage& job_message,
    std::vector<uint8_t>* buffer) const {
  ResponseDataMessage<LockPropertyReply> resDataMsg(job_message);
  const MessageDataLockPropertyReply* reply_data =
      reinterpret_cast<MessageDataLockPropertyReply*>(msg.data);
  resDataMsg.result = reply_data->status.ok;
  resDataMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    resDataMsg.data.resource_id = std::to_string(reply_data->resource_id);
  }
  LOG_D("[reply] LockProperty: handle=%s, message=%s",
      resDataMsg.handle.c_str(), reply_data->status.Get().ToString().c_str());
  SerializeMsg(&resDataMsg, buffer);
}

/**
 * @brief Reserve request of the frame memory for release.
 * @param[in] (header) The message header of communicate between clients/sevrer.
 * @param[in] (frame) Send frame.
 * @return none
 */
void WsConnection::ReserveReleaseFrame(const MessageHeader& header,
    MessageDataFrameLocalMemory *frame, ReleaseFrameList & release_frame) {

  MessageDataReleaseFrameRequest* tmp = new MessageDataReleaseFrameRequest;
  tmp->sequence_number = frame->sequence_number;
  tmp->rawdata_accessed = false;    // kDeliverAddressSizeOnly
                                    // (ReleaseFrameUnused)
//  tmp->rawdata_accessed = true;    // kDeliverAllData
//                                   // (ReleaseFrame)
  Message* msg = new Message();
  msg->data = tmp;
  msg->header.type = kMessageTypeRequest;
  msg->header.data_type = kMessageDataTypeReleaseFrame;
  msg->header.request_id = header.request_id;
  msg->header.server_stream_id = header.server_stream_id;

  release_frame.push_back(msg);
}

void WsConnection::ReleaseReleaseFrameList(ReleaseFrameList & release_frame) {
  if (release_frame.size() > 0) {
    for (auto msg : release_frame) {
      delete reinterpret_cast<MessageDataReleaseFrameRequest*>(msg->data);
      delete msg;
    }
    release_frame.clear();
  }
}

Status WsConnection::EnqReleaseFrame(ReleaseFrameList& release_frame) {
  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "not opened yet");
  }

  AutoMutex auto_mutex_release(mutex_receive_job_);
  for (auto msg : release_frame) {
    struct ReceiveJob job = { ReceiveJobType::RECEIVE_JOB_REPLY, msg };
    receive_job_.push_back(job);
  }
  release_frame.clear();

  SetEvent(cond_receive_job_);

  return Status::OK();
}

/**
 * @brief Send the event to the connected target.
 * @param[in] (msg) The sending message.
 * @return Status object.
 */
Status WsConnection::SendEvent(const Message& msg) {
  if (msg.data == NULL) {
    return Status::OK();  // do nothing
  }
  MessageDataSendEvent* event = static_cast<MessageDataSendEvent*>(msg.data);
  LOG_D("sending event: type=%s", event->event_type.c_str());
  ResponseDataMessage<MessageDataSendEvent> res;
  res.command = SEND_EVENT;
  res.result = true;
  res.data = *event;
  SendConnectedSocket(res, msg);
  return Status::OK();
}

/**
 * @brief Send the frame to the connected target.
 * @param[in] (msg) The sending message.
 * @return Status object.
 */
Status WsConnection::SendFrame(const Message& msg) {
  MessageDataSendFrame* frame =
      reinterpret_cast<MessageDataSendFrame*>(msg.data);
  ResponseDataMessage<Frames> res;

  res.command = SEND_STREAM;
  res.result = true;
#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
  uint64_t stream_id = msg.header.server_stream_id;
  auto shm_manager = ws::SharedMemoryManager::GetInstance();
  bool is_shared_memory = shm_manager->IsSharedMemory(stream_id);
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA

  ReleaseFrameList release_frame;
  release_frame.clear();
  res.data.frames = std::vector<FrameData>(frame->frames.size());
  for (size_t i = 0; i < frame->frames.size(); ++i) {
    MessageDataFrameLocalMemory* data =
        reinterpret_cast<MessageDataFrameLocalMemory*>(&frame->frames[i]);
    ReserveReleaseFrame(msg.header, data, release_frame);

    FrameData& resMsg = res.data.frames[i];
    // frame->GetType(&resMsg.data.type);
    resMsg.SequenceNumber = data->sequence_number;
    resMsg.sequence_number_low = data->sequence_number & 0xffffffff;
    resMsg.sequence_number_high =
        static_cast<uint32_t>(data->sequence_number >> 32);

    resMsg.channel_num = (int32_t)data->channels.size();
    resMsg.channel_list = std::vector<WSF_Channel>(resMsg.channel_num);
    for (int j = 0; j < resMsg.channel_num; j++) {
      MessageDataChannelLocalMemory *raw_data = &data->channels[j];
      WSF_Channel& resMsgChannel = resMsg.channel_list[j];
      resMsgChannel.time_stamp_s =
          static_cast<uint32_t>(raw_data->timestamp / 1000000000);
      resMsgChannel.time_stamp_ns =
          static_cast<uint32_t>(raw_data->timestamp % 1000000000);
      resMsgChannel.id = raw_data->channel_id;
      std::string data_type = raw_data->rawdata_type;
      resMsgChannel.data_type = data_type;

      RawDataInfo info = {};
      if (raw_data->rawdata_info.rawdata.size() > 0) {
        Status status =
          DeserializeFromVector(raw_data->rawdata_info.rawdata, &info);
        SENSCORD_STATUS_TRACE(status);
        if (!status.ok()) {
          LOG_E("failed to decode rawdata info. ch=%d  type=%s",
              raw_data->channel_id, raw_data->rawdata_type.c_str());
        }
      }

      if (info.size > 0) {  // raw data available
        std::string slam_data_format {};
        if (kRawDataTypePose == data_type) {
          GetSlamDataFormat(raw_data->properties, slam_data_format);
        }

        if (kRawDataTypeAcceleration == data_type ||
            kRawDataTypeAngularVelocity == data_type ||
            kRawDataTypeMagneticField == data_type) {
          resMsgChannel.raw_mode = RAW_MODE_BINARY;
          ChannelDataToMsgPack<Vector3<float>>(info, resMsgChannel.raw);
        } else if (kRawDataTypeObjectTracking == data_type) {
          resMsgChannel.raw_mode = RAW_MODE_BINARY;
          ChannelDataToMsgPack<ObjectTrackingData>(info, resMsgChannel.raw);
        } else if (kRawDataTypePose == data_type &&
                 kPoseDataFormatQuaternion == slam_data_format) {
          resMsgChannel.raw_mode = RAW_MODE_BINARY;
          ChannelDataToMsgPack<PoseQuaternionData>(info, resMsgChannel.raw);
        } else if (kRawDataTypePose == data_type &&
                 kPoseDataFormatMatrix == slam_data_format) {
          resMsgChannel.raw_mode = RAW_MODE_BINARY;
          ChannelDataToMsgPack<PoseMatrixData>(info, resMsgChannel.raw);
        } else if (kRawDataTypeKeyPoint == data_type) {
          resMsgChannel.raw_mode = RAW_MODE_BINARY;
          ChannelDataToMsgPack<KeyPointData>(info, resMsgChannel.raw);
        } else {
          uint8_t* src = reinterpret_cast<uint8_t*>(info.src);
#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
          if (is_shared_memory) {
            // Send RawData (shared memory)
            ws::InputData input[3] = {};
            // [0]: sequence number (validation)
            input[0].buffer = &resMsg.SequenceNumber;
            input[0].size = sizeof(resMsg.SequenceNumber);
            // [1]: channel id (validation)
            input[1].buffer = &resMsgChannel.id;
            input[1].size = sizeof(resMsgChannel.id);
            // [2]: raw data
            input[2].buffer = src;
            input[2].size = info.size;
            ws::OutputData output = {};
            Status status = shm_manager->SetData(stream_id, input, 3, &output);
            if (status.ok()) {
              resMsgChannel.raw_mode = RAW_MODE_MAPPED;
              resMsgChannel.mapped_raw_offset = output.offset;
              resMsgChannel.mapped_raw_size = output.size;
            } else {
              // In case of error, send a copy.
              resMsgChannel.raw_mode = RAW_MODE_REF;
              resMsgChannel.raw_ref.ptr = reinterpret_cast<char*>(src);
              resMsgChannel.raw_ref.size = info.size;
            }
          } else {
            resMsgChannel.raw_mode = RAW_MODE_REF;
            resMsgChannel.raw_ref.ptr = reinterpret_cast<char*>(src);
            resMsgChannel.raw_ref.size = static_cast<uint32_t>(info.size);
          }
#else
          resMsgChannel.raw_mode = RAW_MODE_REF;
          resMsgChannel.raw_ref.ptr = reinterpret_cast<char*>(src);
          resMsgChannel.raw_ref.size = static_cast<uint32_t>(info.size);
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA
        }
      } else {
        resMsgChannel.raw_mode = RAW_MODE_NONE;
        resMsgChannel.raw_ref.ptr = nullptr;
        resMsgChannel.raw_ref.size = 0;
      }

      // For Property
      SetProperties(resMsg.channel_list, j, raw_data->properties);
      resMsgChannel.num_property =
        (uint32_t)resMsgChannel.map_property.size();
    }
  }

  SendConnectedSocket(res, msg);

  Status status;
  bool released = false;
  {
    AutoMutex lock(mutex_secondary_);
    if (primary_ != nullptr) {
      status = primary_->EnqReleaseFrame(release_frame);
      if (status.ok()) {
        released = true;
      } else {
        LOG_E("request for release frame to primary failed.");
      }
    }
  }
  if (!released) {
    status = EnqReleaseFrame(release_frame);
    SENSCORD_STATUS_TRACE(status);
  }

  if (release_frame.size() > 0) {
    LOG_E("release_frame failed. release resources. size=%d",
        release_frame.size());
    ReleaseReleaseFrameList(release_frame);
  }

  return status;
}

/**
 * @brief Get Slam Data Format in properties.
 * @param[in] (properties) The properties.
 * @param[out] (format) format string of slam data.
 * @return True means that slam data format found.
 */
bool WsConnection::GetSlamDataFormat(
    std::vector<MessageDataProperty>& properties,
    std::string& format) {

  for (auto it = properties.begin(); it != properties.end(); it++) {
    if (it->key != kPoseDataPropertyKey) {
      continue;
    }
    PoseDataProperty property {};
    Status status = DeserializeFromVector(it->property.data, &property);
    if (status.ok()) {
      format = property.data_format;
    } else {
      LOG_E("failed to decode pose_data_property.");
    }
    return status.ok();
  }
  return false;
}

/**
 * @brief Get propert and set message pack.
 * @param[in] (msg) The sending message.
 * @param[in/out] (vect) Message packed vector.
 * @return Status object.
 */
void WsConnection::GetPropertyToMsgPack(const Message& msg,
    std::vector<uint8_t> &vect,
    JobMessage jobMessage) {

  ResponseDataMessage<std::vector<uint8_t> > resMsg(jobMessage);
  MessageDataGetPropertyReply* reply_data =
      reinterpret_cast<MessageDataGetPropertyReply*>(msg.data);

  LOG_D("GetPropertyToMsgPack():%s, id:%" PRId64
      ", property key:%s, result=%d",
      resMsg.handle.c_str(),
      msg.header.server_stream_id,
      jobMessage.property_key.c_str(),
      reply_data->status.ok);

  resMsg.result = reply_data->status.ok;
  resMsg.status = reply_data->status;
  if (reply_data->status.ok) {
    Status status = ws_bridge_.BinaryToPropertyPack(reply_data->key,
        &reply_data->property.data, &resMsg.data);
    resMsg.result = status.ok();
    LOG_D("reply_data.property.data size=%d, result=%d",
        reply_data->property.data.size(), resMsg.result);
    if (!resMsg.result) {
      LOG_E("cause=%s", status.ToString().c_str());
    }
    resMsg.status.Set(status);
  } else {
    LOG_E("error status: %s", resMsg.status.message.c_str());
  }
  SerializeMsg(&resMsg, &vect);
}

/**
 * @brief Receive the message from the connected target.
 * @param[out] (msg) The receiving message.
 * @return Status object.
 */
Status WsConnection::Recv(Message* msg) {
  if (socket_ == NULL) {
    LOG_E("not opened yet");
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation, "not opened yet");
  }

  if (next_packet_.size() <= 0) {
    struct ReceiveJob job = {};
    size_t que_size = 0;
    {
      AutoMutex auto_mutex_release(mutex_receive_job_);
      que_size = receive_job_.size();
      if (que_size > 0) {
        auto it = receive_job_.begin();
        job = *it;
        receive_job_.erase(it);
      }
    }

    if (que_size == 0) {
      LOG_E("no data and request.");
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidOperation,
          "no data and request.");
    } else if (job.type == ReceiveJobType::RECEIVE_JOB_REPLY) {
      senscord::Message * job_msg =
          reinterpret_cast<senscord::Message *>(job.address);
      msg->data = job_msg->data;
      msg->header.type = job_msg->header.type;
      msg->header.data_type = job_msg->header.data_type;
      msg->header.request_id = job_msg->header.request_id;
      msg->header.server_stream_id = job_msg->header.server_stream_id;
      delete job_msg;

      return Status::OK();
    }
  }

  // receive one message.
  std::vector<uint8_t> payload;
  int length = 0;
  wsFrameType frame_type = readWsMessage(payload, &length);

  {
    AutoMutex auto_mutex(mutex_receive_job_);
    data_available_ = false;
    SetEvent(cond_recvive_finish_);
  }

  if (frame_type == WS_CLOSING_FRAME) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseCancelled, "Disconnected Recieved!!");
  }
  if (WS_BINARY_FRAME != frame_type || length == 0) {
    msg->header.type = kMessageTypeHandshake;
    return Status::OK();
  }

  RequestMessage reqMsg;
  LOG_D("recv deserialize msg");
  Status status = DeserializeMsg(
      reinterpret_cast<char*>(&payload[0]), length, &reqMsg);
  LOG_D("done, status=%d", status.ok());
  if (!status.ok()) {
    return status;
  }
  LOG_D("handle:%s, command:%d, uniq_key:%s, stream_key:%s, property key:%s",
      reqMsg.handle.c_str(),
      reqMsg.command,
      reqMsg.uniq_key.c_str(),
      reqMsg.stream_key.c_str(),
      reqMsg.property_key.c_str());

  uint64_t stream_id = 0;
  {
    AutoMutex auto_mutex(mutex_handle_);
    HandleStreamList::const_iterator itr = handle_stream_.find(reqMsg.handle);
    if (itr != handle_stream_.end()) {
      stream_id = itr->second.stream_id;
    }
  }
  msg->header.server_stream_id = stream_id;

  JobMessage* jobMsg = new JobMessage();
  jobMsg->uniq_key = reqMsg.uniq_key;
  jobMsg->handle = reqMsg.handle;
  jobMsg->command = reqMsg.command;
  jobMsg->index = reqMsg.index;
  jobMsg->stream_key = reqMsg.stream_key;
  jobMsg->property_key = reqMsg.property_key;

  request_id_++;
  msg->header.request_id = request_id_;
  insert(mutex_job_, jobs_, request_id_, jobMsg);

  msg->header.type = kMessageTypeRequest;
  msg->data = NULL;

  switch (reqMsg.command) {
    case GET_STREAM_LIST:
      msg->header.data_type = kMessageDataTypeGetStreamList;
      break;

    case GET_VERSION:
      msg->header.data_type = kMessageDataTypeGetVersion;
      break;

    case OPEN_STREAM:
    {
      senscord::MessageDataOpenRequest* tmp = new MessageDataOpenRequest;
      msg->data = tmp;
      tmp->stream_key = reqMsg.stream_key;
      msg->header.data_type = kMessageDataTypeOpen;
#ifdef USE_SHARED_MEMORY_FOR_RAWDATA
      if (reqMsg.msg_pack_data_exist) {
        serialize::Decoder decoder(
            reqMsg.msg_pack_data.data(), reqMsg.msg_pack_data.size());
        // deserialize extended data.
        OpenStreamRequest extended_data = {};
        Status ret = decoder.Pop(extended_data);
        if (ret.ok()) {
          if (extended_data.use_shared_memory) {
            // Keep the shared memory size.
            if (extended_data.shared_memory_size == 0) {
              extended_data.shared_memory_size = kDefaultSharedMemorySize;
            }
            auto shm_manager = ws::SharedMemoryManager::GetInstance();
            shm_manager->SetSharedMemorySize(
                tmp->stream_key, extended_data.shared_memory_size);
          }
        } else {
          // error log
          LOG_E("Failed to deserialize extended data: %s",
              ret.ToString().c_str());
        }
      }
#endif  // USE_SHARED_MEMORY_FOR_RAWDATA
      break;
    }
    case CLOSE_STREAM:
      msg->header.data_type = kMessageDataTypeClose;
      break;

    case START_STREAM:
      msg->header.data_type = kMessageDataTypeStart;
      break;

    case STOP_STREAM:
      msg->header.data_type = kMessageDataTypeStop;
      break;

    case REGISTER_EVENT:
    {
      msg->header.data_type = kMessageDataTypeRegisterEvent;
      senscord::MessageDataRegisterEventRequest* tmp =
          new MessageDataRegisterEventRequest;
      tmp->event_type = reqMsg.event_type;
      msg->data = tmp;
      LOG_D("RegisterEvent: event:%s handle:%s",
          reqMsg.event_type.c_str(), reqMsg.handle.c_str());
      break;
    }

    case UNREGISTER_EVENT:
    {
      msg->header.data_type = kMessageDataTypeUnregisterEvent;
      senscord::MessageDataUnregisterEventRequest* tmp =
          new MessageDataUnregisterEventRequest;
      tmp->event_type = reqMsg.event_type;
      msg->data = tmp;
      LOG_D("UnregisterEvent: event:%s handle:%s",
          reqMsg.event_type.c_str(), reqMsg.handle.c_str());
      break;
    }

    case GET_PROPERTY:
    {
      senscord::MessageDataGetPropertyRequest* tmp =
          new MessageDataGetPropertyRequest;
      msg->data = tmp;
      tmp->key = reqMsg.property_key;
      msg->header.data_type = kMessageDataTypeGetProperty;
      if (reqMsg.msg_pack_data_exist) {
        Status ret = ws_bridge_.PropertyPackToBinary(
            tmp->key, &reqMsg.msg_pack_data, &tmp->property.data);
        if (!ret.ok()) {
          MessageDataGetPropertyReply reply {};
          reply.status.Set(ret);
          reply.key = reqMsg.property_key;

          Message repMsg {};
          repMsg.header.server_stream_id = msg->header.server_stream_id;
          repMsg.header.request_id = msg->header.request_id;
          repMsg.header.type = kMessageTypeReply;
          repMsg.header.data_type = msg->header.data_type;
          repMsg.data = const_cast<MessageDataGetPropertyReply *>(&reply);

          Send(repMsg);
          return ret;
        }
      }
      break;
    }
    case SET_PROPERTY:
    {
      senscord::MessageDataSetPropertyRequest* tmp =
          new MessageDataSetPropertyRequest;
      msg->data = tmp;
      tmp->key = reqMsg.property_key;
      msg->header.data_type = kMessageDataTypeSetProperty;
      if (reqMsg.msg_pack_data_exist) {
        Status ret = ws_bridge_.PropertyPackToBinary(
            tmp->key, &reqMsg.msg_pack_data, &tmp->property.data);
        if (!ret.ok()) {
          MessageDataSetPropertyReply reply {};
          reply.status.Set(ret);

          Message repMsg {};
          repMsg.header.server_stream_id = msg->header.server_stream_id;
          repMsg.header.request_id = msg->header.request_id;
          repMsg.header.type = kMessageTypeReply;
          repMsg.header.data_type = msg->header.data_type;
          repMsg.data = const_cast<MessageDataSetPropertyReply *>(&reply);

          Send(repMsg);
          return ret;
        }
      }
      break;
    }
    case GET_PROPERTY_LIST:
      msg->header.data_type = kMessageDataTypeGetPropertyList;
      break;

    case OPEN_SECONDARY_CONNECT: {
      // primary handle -> stream_id
      stream_id = 0;
      Status ret = ws::WsConnectionManager::GetInstance()->GetStreamId(
          reqMsg.primary_handle, &stream_id);
      if (!ret.ok()) {
        MessageDataSecondaryConnectReply reply {};
        reply.status.Set(ret);

        Message repMsg {};
        repMsg.header.server_stream_id = msg->header.server_stream_id;
        repMsg.header.request_id = msg->header.request_id;
        repMsg.header.type = kMessageTypeReply;
        repMsg.header.data_type = kMessageDataTypeSecondaryConnect;
        repMsg.data = const_cast<MessageDataSecondaryConnectReply *>(&reply);

        Send(repMsg);
        return ret;
      }

      msg->header.server_stream_id = stream_id;
      msg->header.data_type = kMessageDataTypeSecondaryConnect;
      msg->data = new MessageDataSecondaryConnectRequest;
      break;
    }
    case LOCK_PROPERTY:
    {
      msg->header.data_type = kMessageDataTypeLockProperty;
      senscord::MessageDataLockPropertyRequest* tmp =
          new MessageDataLockPropertyRequest;
      tmp->keys = reqMsg.keys;
      tmp->timeout_msec = reqMsg.timeout_msec;
      msg->data = tmp;
      LOG_D("LockProperty: handle:%s", reqMsg.handle.c_str());
      break;
    }
    case UNLOCK_PROPERTY:
    {
      msg->header.data_type = kMessageDataTypeUnlockProperty;
      senscord::MessageDataUnlockPropertyRequest* tmp =
          new MessageDataUnlockPropertyRequest;
      tmp->resource_id = std::stoull(reqMsg.resource);
      msg->data = tmp;
      LOG_D("UnlockProperty: handle:%s", reqMsg.handle.c_str());
      break;
    }
    default: {
      // unknown stream
      LOG_W("unknown command: %d", reqMsg.command);
      {
        JobList::const_iterator itr;
        class AutoMutex mutex_job(mutex_job_);
        itr = jobs_.find(msg->header.request_id);
        if (itr == jobs_.end()) {
          LOG_D("recv job not found");
        } else {
          jobs_.erase(itr);
          LOG_D("recv job erased");
        }
      }
      delete jobMsg;
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation, "unknown WebBridge command: %d",
          reqMsg.command);
    }
  }
  LOG_D("return to ClientAdapter() data_type=%d", msg->header.data_type);
  return Status::OK();
}

/**
 * @brief Releases message data generated by Recv function.
 * @param[in] (msg_header) Message header.
 * @param[in] (msg_data) Pointer to message data to release.
 * @return Status object.
 */
Status WsConnection::ReleaseMessage(
    const MessageHeader& msg_header, void* msg_data) const {
  if (msg_data == NULL) {
    // do nothing.
    return Status::OK();
  }

  Status status;
  switch (msg_header.type) {
    case kMessageTypeSendFrame: {
      MessageDataSendFrame* tmp =
          reinterpret_cast<MessageDataSendFrame*>(msg_data);
      delete tmp;
      break;
    }
    case kMessageTypeSendEvent: {
      MessageDataSendEvent* tmp =
          reinterpret_cast<MessageDataSendEvent*>(msg_data);
      delete tmp;
      break;
    }
    case kMessageTypeHandshake:
      break;
    case kMessageTypeRequest: {
      switch (msg_header.data_type) {
        case kMessageDataTypeOpen: {
          MessageDataOpenRequest* tmp =
              reinterpret_cast<MessageDataOpenRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeGetProperty: {
          MessageDataGetPropertyRequest* tmp =
              reinterpret_cast<MessageDataGetPropertyRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeSetProperty: {
          MessageDataSetPropertyRequest* tmp =
              reinterpret_cast<MessageDataSetPropertyRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeReleaseFrame: {
          MessageDataReleaseFrameRequest* tmp =
              reinterpret_cast<MessageDataReleaseFrameRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeSecondaryConnect: {
          MessageDataSecondaryConnectRequest* tmp =
              reinterpret_cast<MessageDataSecondaryConnectRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeRegisterEvent: {
          MessageDataRegisterEventRequest* tmp =
              reinterpret_cast<MessageDataRegisterEventRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeUnregisterEvent: {
          MessageDataUnregisterEventRequest* tmp =
              reinterpret_cast<MessageDataUnregisterEventRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeLockProperty: {
          MessageDataLockPropertyRequest* tmp =
              reinterpret_cast<MessageDataLockPropertyRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeUnlockProperty: {
          MessageDataUnlockPropertyRequest* tmp =
              reinterpret_cast<MessageDataUnlockPropertyRequest*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeClose:
        case kMessageDataTypeStart:
        case kMessageDataTypeStop:
        case kMessageDataTypeGetStreamList:
        case kMessageDataTypeGetVersion:
        case kMessageDataTypeGetPropertyList:
          break;
        default:
          status = SENSCORD_STATUS_FAIL(
              kStatusBlockCore, Status::kCauseInvalidArgument,
              "invalid MessageDataType: type=Request, data_type=%" PRIu32,
              msg_header.data_type);
          LOG_E("%s", status.ToString().c_str());
          break;
      }
      break;
    }
    case kMessageTypeReply: {
      switch (msg_header.data_type) {
        case kMessageDataTypeOpen: {
          MessageDataOpenReply* tmp =
              reinterpret_cast<MessageDataOpenReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeClose: {
          MessageDataCloseReply* tmp =
              reinterpret_cast<MessageDataCloseReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeStart: {
          MessageDataStartReply* tmp =
              reinterpret_cast<MessageDataStartReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeStop: {
          MessageDataStopReply* tmp =
              reinterpret_cast<MessageDataStopReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeReleaseFrame: {
          MessageDataReleaseFrameReply* tmp =
              reinterpret_cast<MessageDataReleaseFrameReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeGetProperty: {
          MessageDataGetPropertyReply* tmp =
              reinterpret_cast<MessageDataGetPropertyReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeSetProperty: {
          MessageDataSetPropertyReply* tmp =
              reinterpret_cast<MessageDataSetPropertyReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeLockProperty: {
          MessageDataLockPropertyReply* tmp =
              reinterpret_cast<MessageDataLockPropertyReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeUnlockProperty: {
          MessageDataUnlockPropertyReply* tmp =
              reinterpret_cast<MessageDataUnlockPropertyReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeSendFrame: {
          MessageDataSendFrameReply* tmp =
              reinterpret_cast<MessageDataSendFrameReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeGetStreamList: {
          MessageDataStreamListReply* tmp =
              reinterpret_cast<MessageDataStreamListReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeGetVersion: {
          MessageDataVersionReply* tmp =
              reinterpret_cast<MessageDataVersionReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeGetPropertyList: {
          MessageDataPropertyListReply* tmp =
              reinterpret_cast<MessageDataPropertyListReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeSecondaryConnect: {
          MessageDataSecondaryConnectReply* tmp =
              reinterpret_cast<MessageDataSecondaryConnectReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeRegisterEvent: {
          MessageDataRegisterEventReply* tmp =
              reinterpret_cast<MessageDataRegisterEventReply*>(msg_data);
          delete tmp;
          break;
        }
        case kMessageDataTypeUnregisterEvent: {
          MessageDataUnregisterEventReply* tmp =
              reinterpret_cast<MessageDataUnregisterEventReply*>(msg_data);
          delete tmp;
          break;
        }
        default:
          status = SENSCORD_STATUS_FAIL(
              kStatusBlockCore, Status::kCauseInvalidArgument,
              "invalid MessageDataType: type=Reply, data_type=%" PRIu32,
              msg_header.data_type);
          LOG_E("%s", status.ToString().c_str());
          break;
      }
      break;
    }
    default:
      status = SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseInvalidArgument,
          "invalid MessageType: type=0x%" PRIx32 ", data_type=%" PRIu32,
          msg_header.type, msg_header.data_type);
      LOG_E("%s", status.ToString().c_str());
      break;
  }

  return status;
}

/**
 * @brief Get raw data for SendFrame of the server.
 * @param[in] (channel) Channel object.
 * @param[out] (rawdata) Information of the raw data to send.
 * @return Status object.
 */
Status WsConnection::GetChannelRawData(
    const Channel* channel, ChannelRawDataInfo* rawdata) const {
  if (rawdata == NULL) {
    LOG_E("rawdata is null");
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "rawdata is null");
  }

  MemoryContained rawdata_memory = {};
  Status status = channel->GetRawDataMemory(&rawdata_memory);
  if (!status.ok()) {
    LOG_E("%s", status.ToString().c_str());
    return SENSCORD_STATUS_TRACE(status);
  }

  if (rawdata_memory.memory == NULL) {
    // Do nothing.
    return Status::OK();
  }

  {
    // all copy.
    rawdata->delivering_mode = kDeliverAllData;
    RawDataInfo info;
    uint8_t *src = reinterpret_cast<uint8_t*>(
        rawdata_memory.memory->GetAddress()) + rawdata_memory.offset;
    info.src = (uint64_t)src;
    info.size = rawdata_memory.size;
    status = SerializeToVector(&info, &rawdata->rawdata);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Wait to be readable this connection.
 * @param[in] (timeout) Nanoseconds for wating.
 * @return If be readable then return ok.
 */
Status WsConnection::WaitReadable(uint64_t timeout) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidOperation,
        "not opened yet");
  }

  listening_flag_ = true;

  if (next_packet_.size() > 0) {
    return Status::OK();
  }

  {
    AutoMutex auto_mutex_release(mutex_receive_job_);
    if (receive_job_.size() > 0) {
      return Status::OK();
    }
    WaitEvent(mutex_receive_job_, cond_receive_job_, timeout);
    if (receive_job_.size() <= 0) {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseTimeout,
          "timeout to wait readable. no received data and no request.");
    }
  }

  return Status::OK();
}

/**
 * @brief Get the address from parameter.
 * @param[in] (param) The parameter of bind or connect.
 * @return The address for osal socket.
 */
osal::OSSocketAddressInet WsConnection::GetAddress(
    const std::string& param) const {
  osal::OSSocketAddressInet addr = {osal::kOSInAddrAny, 0};
  if (!param.empty()) {
    std::string ipadr;

    // split ipadr and port
    {
      std::stringstream ss(param);
      std::getline(ss, ipadr, ':');
      ss >> addr.port;
      addr.port = osal::OSHtons(addr.port);
    }

    // split ipadr
    if (!ipadr.empty()) {
      osal::OSInetAton(ipadr.c_str(), &addr.address);
    }
  }
  return addr;
}

/**
 * @brief Create palyload length array for web socket frame header.
 * @param[in] (type) frame type.
 * @param[in] (length) frame length.
 * @return length of payload length array.
 */
size_t WsConnection::CreatePayloadLength(
    const enum wsFrameType type, const size_t length,
    uint8_t (& outFrame)[10]) {
  size_t outLength = 0;
  outFrame[0] = 0x80 | (uint8_t)type;

  if (length <= 125) {
    outFrame[1] = (uint8_t)length;
    outLength = 2;
  } else if (length <= 0xFFFF) {
    outFrame[1] = 126;
    outFrame[2] = (length >> 8) & 0xff;
    outFrame[3] = length & 0xff;
    outLength = 4;
  } else if (length <= 0xFFFFFFFF) {
    outFrame[1] = 127;
    outFrame[2] = 0;
    outFrame[3] = 0;
    outFrame[4] = 0;
    outFrame[5] = 0;
    outFrame[6] = (length >> 24) & 0xff;
    outFrame[7] = (length >> 16) & 0xff;
    outFrame[8] = (length >> 8) & 0xff;
    outFrame[9] = length & 0xff;
    outLength = 10;
  } else {
    LOG_W("Not support. Lengh is greater than 0xFFFFFFFF.");
  }

  return outLength;
}

/**
 * @brief Set the primary connection.
 * @param[in] (connection) The primary connection.
 */
void WsConnection::SetPrimary(WsConnection* connection) {
  AutoMutex lock(mutex_secondary_);
  if (primary_ != connection) {
    LOG_D("[%p] Primary: %p -> %p", this, primary_, connection);
    primary_ = connection;
  }
}

/**
 * @brief Set the secondary connection.
 * @param[in] (connection) The secondary connection.
 */
void WsConnection::SetSecondary(WsConnection* connection) {
  AutoMutex lock(mutex_secondary_);
  if (secondary_ != connection) {
    LOG_D("[%p] Secondary: %p -> %p", this, secondary_, connection);
    secondary_ = connection;
  }
}

/**
 * @brief Constructor.
 */
WsConnection::WsConnection()
    : socket_(), address_(), request_id_(),
      mutex_(), mutex_job_(), mutex_handle_(), mutex_receive_job_(),
      thread_(), end_flag_(), listening_flag_(),
      cond_receive_job_(), cond_recvive_finish_(), data_available_(),
      state_(WS_STATE_OPENING), hs_(),
      primary_(), secondary_(), mutex_secondary_() {
  CommonInit();
}

/**
 * @brief Constructor with the created socket.
 * @param[in] (socket) The created socket.
 * @param[in] (address) The incoming address.
 */
WsConnection::WsConnection(
    osal::OSSocket* socket, const osal::OSSocketAddressInet& address)
    : socket_(socket), address_(address), request_id_(),
      mutex_(), mutex_job_(), mutex_handle_(), mutex_receive_job_(),
      thread_(), end_flag_(), listening_flag_(),
      cond_receive_job_(), cond_recvive_finish_(), data_available_(),
      state_(WS_STATE_OPENING), hs_(),
      primary_(), secondary_(), mutex_secondary_() {
  CommonInit();
}

/**
 * @brief Destructor.
 */
WsConnection::~WsConnection() {
  LOG_D("[%p] ~WsConnection(): --> enter socket=0x%x", this, socket_);
  Stop();
  Close();
  DestroyMutex();
  LOG_D("[%p] ~WsConnection(): <-- leave", this);
}

/**
 * @brief Common Initialization
 */
void WsConnection::CommonInit() {
  CreateMutex();
  next_packet_.clear();
  receive_job_.clear();
  nullHandshake(&hs_);
  end_flag_ = false;
  listening_flag_ = false;
  thread_ = NULL;
  data_available_ = false;
  Start();
}

/**
 * @brief Create mutex resources
 */
void WsConnection::CreateMutex() {
  osal::OSCreateMutex(&mutex_);
  osal::OSCreateMutex(&mutex_job_);
  osal::OSCreateMutex(&mutex_handle_);
  osal::OSCreateMutex(&mutex_receive_job_);
  osal::OSCreateMutex(&mutex_secondary_);
  osal::OSCreateCond(&cond_receive_job_);
  osal::OSCreateCond(&cond_recvive_finish_);
}

/**
 * @brief Destroy mutex resources
 */
void WsConnection::DestroyMutex() {
  if (cond_receive_job_ != NULL) {
    osal::OSDestroyCond(cond_receive_job_);
    cond_receive_job_ = NULL;
  }
  if (cond_recvive_finish_ != NULL) {
    osal::OSDestroyCond(cond_recvive_finish_);
    cond_recvive_finish_ = NULL;
  }
  if (mutex_ != NULL) {
    osal::OSDestroyMutex(mutex_);
    mutex_ = NULL;
  }
  if (mutex_job_ != NULL) {
    osal::OSDestroyMutex(mutex_job_);
    mutex_job_ = NULL;
  }
  if (mutex_handle_ != NULL) {
    osal::OSDestroyMutex(mutex_handle_);
    mutex_handle_ = NULL;
  }
  if (mutex_receive_job_ != NULL) {
    osal::OSDestroyMutex(mutex_receive_job_);
    mutex_receive_job_ = NULL;
  }
  if (mutex_secondary_ != NULL) {
    osal::OSDestroyMutex(mutex_secondary_);
    mutex_secondary_ = NULL;
  }
}

}   // namespace senscord
