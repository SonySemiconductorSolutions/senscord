/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senscord/develop/client_messenger.h"
#include <inttypes.h>
#include <string>
#include <limits>
#include <map>
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/connection.h"
#include "senscord/connection_manager.h"
#include "senscord/logger.h"
#include "util/autolock.h"

namespace senscord {

// waiting time
static const uint64_t kRecvWaitTime = 1000ULL * 1000 * 1000;   // 1sec

/**
 * @brief The thread processing for receive messages.
 * @param[in] (arg) Client messenger pointer.
 * @return Don't care.
 */
static osal::OSThreadResult RecvThread(void* arg) {
  if (arg) {
    ClientMessenger* messenger = reinterpret_cast<ClientMessenger*>(arg);
    messenger->RecvThreadCore();
  }
  return static_cast<osal::OSThreadResult>(0);
}

/**
 * @brief Start the messaging.
 * @param[in] (connection_mode) The connection mode.
 * @param[in] (address_primary) Primary destination address.
 * @param[in] (address_secondary) Secondary destination address.
 * @return Status object.
 */
Status ClientMessenger::Start(
    const std::string& connection_mode,
    const std::string& address_primary,
    const std::string& address_secondary) {
  Status status;
  if (connection_ == NULL) {
    // create new connection.
    Connection* connection = NULL;
    status = ConnectionManager::GetInstance()->CreateConnection(
        connection_mode, &connection);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
    connection_ = connection;
  }

  std::map<std::string, std::string> arguments;
  ConnectionManager::GetInstance()->GetArguments(
      connection_mode, &arguments);

  status = connection_->Open(arguments);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  status = connection_->Connect(address_primary);
  if (!status.ok()) {
    connection_->Close();
    return SENSCORD_STATUS_TRACE(status);
  }

  // start recv thread
  SetConnectStatus(true);
  int32_t ret = osal::OSCreateThread(&recv_thread_,
      RecvThread, this, NULL);
  if (ret != 0) {
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseAborted,
        "failed to OSCreateThread: %" PRIx32, ret);
  }

  SENSCORD_LOG_INFO("[messenger] connect server: [%s]%s",
      connection_mode.c_str(), address_primary.c_str());

  // secondary connection
  if (!address_secondary.empty()) {
    ClientMessenger* child = new ClientMessenger;
    child->RegisterFrameCallback(frame_callback_, frame_callback_arg_);
    status = child->Start(connection_mode, address_secondary, "");
    if (status.ok()) {
      child_ = child;
    } else {
      SENSCORD_STATUS_TRACE(status);
      delete child;
    }
  }

  return status;
}

/**
 * @brief Stop the messaging.
 * @return Status object.
 */
Status ClientMessenger::Stop() {
  if (child_ != NULL) {
    // clear server stream id
    child_->ClearServerStreamId();

    child_->Stop();
    delete child_;
    child_ = NULL;
  }

  if (recv_thread_) {
    // disconnect
    Status status = RequestDisconnection();
    if (!status.ok()) {
      SENSCORD_LOG_WARNING(
          "[messenger] failed to disconnect server");
      // if failed sending, stop force.
      SetConnectStatus(false);
    } else {
      SENSCORD_LOG_DEBUG("[messenger] wait the reply to disconnect.");
    }

    // wait to finish the recv
    osal::OSJoinThread(recv_thread_, NULL);
    recv_thread_ = NULL;
  }

  {
    util::AutoLock lock(mutex_waiting_);
    WaitingCommandQueue::iterator itr = waiting_commands_.begin();
    WaitingCommandQueue::const_iterator end = waiting_commands_.end();
    for (; itr != end; ++itr) {
      WaitingCommand* waiting = (*itr);
      osal::OSDestroyCond(waiting->cond);
      ReleaseCommandReply(waiting->reply_msg);
      delete waiting;
    }
    waiting_commands_.clear();
  }

  // clear server stream id
  ClearServerStreamId();

  if (connection_ != NULL) {
    // release connection.
    Status status;
    status = ConnectionManager::GetInstance()->ReleaseConnection(
        connection_);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
    connection_ = NULL;
  }

  return Status::OK();
}

/**
 * @brief Make a secondary connection.
 * @param[in] (port_type) Client port type when opened.
 * @param[in] (port_id) Client port id when opened.
 * @param[in] (stream_id) Stream ID from server.
 * @param[in] (timeout_nsec) Timeout nanoseconds. (0: infinite)
 * @return Status object.
 */
Status ClientMessenger::MakeSecondaryConnection(
    const std::string& port_type, int32_t port_id, uint64_t stream_id,
    uint64_t timeout_nsec) {
  Status status;
  if (child_ != NULL) {
    MessageDataSecondaryConnectRequest msg_data = {};
    Message msg = {};
    msg.header.server_stream_id = stream_id;
    msg.header.request_id = child_->GetRequestId();
    msg.header.type = kMessageTypeRequest;
    msg.header.data_type = kMessageDataTypeSecondaryConnect;
    msg.data = &msg_data;

    // send request
    status = child_->SendCommandRequest(msg);
    SENSCORD_STATUS_TRACE(status);

    Message* reply = NULL;

    if (status.ok()) {
      // wait reply
      status = child_->WaitCommandReply(
          msg.header.request_id, timeout_nsec, &reply);
      SENSCORD_STATUS_TRACE(status);
    }

    if (status.ok()) {
      // cast reply payload
      const MessageDataSecondaryConnectReply& reply_data =
          *reinterpret_cast<const MessageDataSecondaryConnectReply*>(
              reply->data);

      // check return status.
      status = reply_data.status.Get();
      SENSCORD_STATUS_TRACE(status);

      if (status.ok()) {
        // add server info
        child_->AddServerStreamId(port_type, port_id, stream_id);
        SENSCORD_LOG_INFO(
            "Successful secondary connection. stream id: %" PRIx64,
            stream_id);
      }

      // release reply
      child_->ReleaseCommandReply(reply);
    }
  }
  return status;
}

/**
 * @brief Send the request message.
 * @param[in] (msg) The message of request.
 * @return Status object.
 */
Status ClientMessenger::SendCommandRequest(
    const Message& msg) {
  // register waiting request id of reply
  Status status = RegisterWaitCommand(msg.header.request_id);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    return status;
  }

  // send
  status = connection_->Send(msg);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    UnregisterWaitCommand(msg.header.request_id);
  }
  return status;
}

/**
 * @brief Wait the reply message of request.
 * @param[in] (request_id) Requested ID.
 * @param[in] (timeout_nsec) Timeout nanoseconds. (0: infinite)
 * @param[out] (msg) The reply message. When used, must be released.
 * @return Status object.
 */
Status ClientMessenger::WaitCommandReply(
    uint64_t request_id, uint64_t timeout_nsec, Message** msg) {
  if (msg == NULL) {
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseInvalidArgument, "msg is null");
  }
  Message* reply = NULL;

  {
    util::AutoLock lock(mutex_waiting_);
    WaitingCommand* waiting = GetWaitCommand(request_id);
    if (waiting) {
      uint64_t absolute_timeout = 0;
      if (timeout_nsec != 0) {
        osal::OSGetTime(&absolute_timeout);
        if (absolute_timeout <=
            (std::numeric_limits<uint64_t>::max() - timeout_nsec)) {
          // (absolute_timeout + timeout_nsec) <= uint64_max
          absolute_timeout += timeout_nsec;
        } else {
          // uint64_max overflow
          absolute_timeout = 0;  // infinite
        }
      }

      while (IsConnected() && waiting->reply_msg == NULL) {
        int32_t ret = 0;
        if (absolute_timeout == 0) {
          ret = osal::OSWaitCond(waiting->cond, mutex_waiting_->GetObject());
        } else {
          ret = osal::OSTimedWaitCond(
              waiting->cond, mutex_waiting_->GetObject(), absolute_timeout);
          if (osal::error::IsTimeout(ret)) {
            SENSCORD_LOG_WARNING(
                "WaitCommandReply timeout: request_id=%" PRIu64 "\n",
                request_id);
            break;
          }
        }
        if (osal::error::IsError(ret)) {
          SENSCORD_LOG_ERROR(
              "WaitCommandReply failed: request_id=%" PRIu64 ", error=%" PRIx32
              "\n", request_id, ret);
          break;
        }
      }
      reply = waiting->reply_msg;
      // Do not release the message.
      waiting->reply_msg = NULL;
      UnregisterWaitCommand(request_id);
    }
  }

  if (reply == NULL) {
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseNotFound,
        "no reply message found: request_id=%" PRIu64, request_id);
  }
  if (reply->data == NULL) {
    ReleaseCommandReply(reply);
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseInvalidOperation,
        "failed to receive reply (reply->data is null)");
  }
  *msg = reply;
  return Status::OK();
}

/**
 * @brief Release the reply message.
 * @param[in] (msg) The reply message.
 */
void ClientMessenger::ReleaseCommandReply(Message* msg) {
  if (msg != NULL) {
    connection_->ReleaseMessage(msg->header, msg->data);
  }
  delete msg;
}

/**
 * @brief Send the reply message.
 * @param[in] (msg) The message of reply.
 * @return Status object.
 */
Status ClientMessenger::SendCommandReply(
    const Message& msg) {
  Status status = connection_->Send(msg);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Send the send frame message.
 * @param[in] (msg) The message of send frame.
 * @return Status object.
 */
Status ClientMessenger::SendCommandSendFrame(
    const Message& msg) {
  Status status = connection_->Send(msg);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Register the callback for arrived frame.
 * @param[in] (callback) The callback function.
 * @param[in] (arg) The argument when callback called.
 */
void ClientMessenger::RegisterFrameCallback(
    const OnMesageReceivedCallback callback, void* arg) {
  frame_callback_ = callback;
  frame_callback_arg_ = arg;
}

/**
 * @brief Register the callback for arrived event.
 * @param[in] (callback) The callback function.
 * @param[in] (arg) The argument when callback called.
 */
void ClientMessenger::RegisterEventCallback(
    const OnMesageReceivedCallback callback, void* arg) {
  event_callback_ = callback;
  event_callback_arg_ = arg;
}

/**
 * @brief Register the callback for arrived event.
 * @param[in] (callback) The callback function.
 * @param[in] (arg) The argument when callback called.
 */
void ClientMessenger::RegisterRequestCallback(
    const OnMesageReceivedCallback callback, void* arg) {
  request_callback_ = callback;
  request_callback_arg_ = arg;
}

/**
 * @brief The method of receiving thread.
 */
void ClientMessenger::RecvThreadCore() {
  SENSCORD_LOG_DEBUG("[messenger] start monitoring");

  while (IsConnected()) {
    Status status = connection_->WaitReadable(kRecvWaitTime);
    if (IsConnected() && (status.ok())) {
      // recv
      Message* msg = new Message();
      status = connection_->Recv(msg);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok()) {
        // processing messages.
        DealMessage(msg);
      } else if (status.cause() == Status::kCauseCancelled) {
        // disconnected
        SENSCORD_LOG_ERROR("[messenger] disconnect server");
        SetConnectStatus(false);
        ReleaseCommandReply(msg);
        break;
      } else {
        SENSCORD_LOG_WARNING(status.ToString().c_str());
        ReleaseCommandReply(msg);
      }
    } else {
      // no message incoming
    }
  }

  // disconnect
  Status status = connection_->Close();
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    SENSCORD_LOG_WARNING(status.ToString().c_str());
  }

  // wakeup to all waiting process.
  {
    util::AutoLock lock(mutex_waiting_);
    WaitingCommandQueue::iterator itr = waiting_commands_.begin();
    WaitingCommandQueue::const_iterator end = waiting_commands_.end();
    for (; itr != end; ++itr) {
      WaitingCommand* waiting = (*itr);
      osal::OSSignalCond(waiting->cond);
    }
  }

  SENSCORD_LOG_DEBUG("[messenger] end monitoring");
}

/**
 * @brief Dealing the received message.
 * @param[in] (msg) The new incoming message.
 */
void ClientMessenger::DealMessage(Message* msg) {
  if (msg == NULL) {
    return;
  }

  SENSCORD_LOG_DEBUG("incoming new message: "
      "func=%d, type=%d, request_id=%" PRIu64 ", stream_id=%" PRIx64,
      msg->header.data_type, msg->header.type, msg->header.request_id,
      msg->header.server_stream_id);

  bool is_keep_msg = false;

  switch (msg->header.type) {
    case kMessageTypeReply:
      if (msg->header.data_type == kMessageDataTypeDisconnect) {
        // start disconnecting
        SENSCORD_LOG_DEBUG("[messenger] disconnecting.");
        UnregisterWaitCommand(msg->header.request_id);
        SetConnectStatus(false);
      } else if (msg->header.data_type == kMessageDataTypeSendFrame) {
        if (request_callback_ != NULL) {
          std::string type;
          int32_t id = -1;
          Status status = GetPortID(
              msg->header.server_stream_id, &type, &id);
          if (status.ok()) {
            request_callback_(type, id, msg, request_callback_arg_);
            is_keep_msg = true;
          } else {
            SENSCORD_LOG_WARNING(
                "[messenger] Failed to request messeage: %s",
                status.ToString().c_str());
          }
        }
      } else {
        // search waiting queue
        util::AutoLock lock(mutex_waiting_);
        WaitingCommand* waiting = GetWaitCommand(msg->header.request_id);
        if (waiting) {
          waiting->reply_msg = msg;
          osal::OSSignalCond(waiting->cond);
          is_keep_msg = true;
        } else {
          // TODO: unknown request id
        }
      }
      break;

    case kMessageTypeSendFrame:
      if (frame_callback_ != NULL) {
        std::string type;
        int32_t id = -1;
        Status status = GetPortID(
            msg->header.server_stream_id, &type, &id);
        if (status.ok()) {
          frame_callback_(type, id, msg, frame_callback_arg_);
          is_keep_msg = true;
        } else {
          SENSCORD_LOG_WARNING(
              "[messenger] Failed to SendFrame: %s",
              status.ToString().c_str());
        }
      }
      break;

    case kMessageTypeSendEvent:
      if (event_callback_ != NULL) {
        std::string type;
        int32_t id = -1;
        Status status = GetPortID(
            msg->header.server_stream_id, &type, &id);
        if (status.ok()) {
          event_callback_(type, id, msg, event_callback_arg_);
          is_keep_msg = true;
        } else {
          SENSCORD_LOG_WARNING(
              "[messenger] Failed to SendEvent: %s",
              status.ToString().c_str());
        }
      }
      break;

    case kMessageTypeRequest:
      if (request_callback_ != NULL) {
        std::string type;
        int32_t id = -1;
        Status status = GetPortID(
            msg->header.server_stream_id, &type, &id);
        if (status.ok()) {
          request_callback_(type, id, msg, request_callback_arg_);
          is_keep_msg = true;
        } else {
          SENSCORD_LOG_WARNING(
              "[messenger] Failed to request messeage: %s",
              status.ToString().c_str());
        }
      }
      break;

    default:
      // unknown message
      break;
  }

  if (!is_keep_msg) {
    ReleaseCommandReply(msg);
  }
}

/**
 * @brief Send the request of disconnet to server.
 * @return Status object.
 */
Status ClientMessenger::RequestDisconnection() {
  // message payload
  MessageDataDisconnectRequest msg_data = {};

  // create message
  Message msg = {};
  msg.header.server_stream_id = kInvalidServerStreamId;
  msg.header.request_id = GetRequestId();
  msg.header.type = kMessageTypeRequest;
  msg.header.data_type = kMessageDataTypeDisconnect;
  msg.data = &msg_data;

  // send to server
  SENSCORD_LOG_DEBUG("[messenger] request to disconnect.");
  Status status = SendCommandRequest(msg);
  if (!status.ok()) {
    SENSCORD_STATUS_TRACE(status);
    SENSCORD_LOG_ERROR("[messenger] failed to request disconnect: %s",
        status.ToString().c_str());
  }
  return status;
}

/**
 * @brief Register to wait the reply message.
 * @param[in] (request_id) The requested ID.
 * @return Status object.
 */
Status ClientMessenger::RegisterWaitCommand(uint64_t request_id) {
  WaitingCommand* waiting = new WaitingCommand();

  // setup wating information
  waiting->request_id = request_id;
  osal::OSCreateCond(&waiting->cond);
  waiting->reply_msg = NULL;

  // registering
  {
    util::AutoLock lock(mutex_waiting_);
    waiting_commands_.push_back(waiting);
  }
  return Status::OK();
}

/**
 * @brief Unregister to wait the reply message.
 * @param[in] (request_id) The requested ID.
 */
void ClientMessenger::UnregisterWaitCommand(uint64_t request_id) {
  util::AutoLock lock(mutex_waiting_);
  WaitingCommandQueue::iterator itr = waiting_commands_.begin();
  WaitingCommandQueue::const_iterator end = waiting_commands_.end();
  for (; itr != end; ++itr) {
    WaitingCommand* waiting = (*itr);
    if (waiting->request_id == request_id) {
      waiting_commands_.erase(itr);
      osal::OSDestroyCond(waiting->cond);
      ReleaseCommandReply(waiting->reply_msg);
      delete waiting;
      break;
    }
  }
}

/**
 * @brief Get the waiting reply information.
 * @param[in] (request_id) The requested ID.
 * @return The waiting information. If no waiting then return NULL.
 */
ClientMessenger::WaitingCommand* ClientMessenger::GetWaitCommand(
    uint64_t request_id) {
  WaitingCommandQueue::iterator itr = waiting_commands_.begin();
  WaitingCommandQueue::const_iterator end = waiting_commands_.end();
  for (; itr != end; ++itr) {
    WaitingCommand* waiting = (*itr);
    if (waiting->request_id == request_id) {
      return waiting;
    }
  }
  return NULL;
}

/**
 * @brief Get the next request ID.
 * @return The next ID.
 */
uint64_t ClientMessenger::GetRequestId() {
  uint64_t ret;
  osal::OSLockMutex(mutex_request_id_);
  ret = request_id_++;
  osal::OSUnlockMutex(mutex_request_id_);
  return ret;
}

/**
 * @brief Set the connection state.
 * @param[in] (status) The connection state.
 */
void ClientMessenger::SetConnectStatus(bool status) {
  util::AutoLock lock(mutex_waiting_);
  is_connected_ = status;
}

/**
 * @brief Get the connection state.
 * @return The connection state.
 */
bool ClientMessenger::IsConnected() const {
  util::AutoLock lock(mutex_waiting_);
  return is_connected_;
}

/**
 * @brief Add the server stream id information.
 * @param[in] (port_type) Client port type when opened.
 * @param[in] (port_id) Client port id when opened.
 * @param[in] (server_stream_id) Stream ID from server.
 */
void ClientMessenger::AddServerStreamId(
    const std::string& port_type, int32_t port_id, uint64_t server_stream_id) {
  ServerStreamId info = {};
  info.port_type = port_type;
  info.port_id = port_id;
  info.server_stream_id = server_stream_id;
  osal::OSLockMutex(mutex_server_stream_id_);
  server_stream_ids_.push_back(info);
  osal::OSUnlockMutex(mutex_server_stream_id_);
}

/**
 * @brief Get the server stream id.
 * @param[in] (port_type) Client port type when opened.
 * @param[in] (port_id) Client port id when opened.
 * @return The server stream id.
 *         If no registered then return kInvalidServerStreamId.
 */
uint64_t ClientMessenger::GetServerStreamId(
    const std::string& port_type,
    int32_t port_id) {
  uint64_t ret = kInvalidServerStreamId;
  osal::OSLockMutex(mutex_server_stream_id_);
  ServerStreamIdList::const_iterator itr = server_stream_ids_.begin();
  ServerStreamIdList::const_iterator end = server_stream_ids_.end();
  for (; itr != end; ++itr) {
    if ((itr->port_type == port_type) && (itr->port_id == port_id)) {
      ret = itr->server_stream_id;
      break;
    }
  }
  osal::OSUnlockMutex(mutex_server_stream_id_);
  return ret;
}

/**
 * @brief Get the port id from the server stream id.
 * @param[in] (server_stream_id) The server stream id.
 * @param[out] (port_type) Client port type when opened.
 * @param[out] (port_id) Client port id when opened.
 * @return Status object.
 */
Status ClientMessenger::GetPortID(
    uint64_t server_stream_id,
    std::string* port_type,
    int32_t* port_id) {
  if (server_stream_id == kInvalidServerStreamId) {
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseInvalidArgument,
        "invalid server stream id");
  }
  if ((port_type == NULL) || (port_id == NULL)) {
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseInvalidArgument,
        "parameter is null");
  }

  osal::OSLockMutex(mutex_server_stream_id_);
  ServerStreamIdList::const_iterator itr = server_stream_ids_.begin();
  ServerStreamIdList::const_iterator end = server_stream_ids_.end();
  for (; itr != end; ++itr) {
    if (itr->server_stream_id == server_stream_id) {
      *port_type = itr->port_type;
      *port_id = itr->port_id;
      osal::OSUnlockMutex(mutex_server_stream_id_);
      return Status::OK();
    }
  }
  osal::OSUnlockMutex(mutex_server_stream_id_);
    return SENSCORD_STATUS_FAIL("messenger",
        Status::kCauseNotFound,
        "unknown server stream id: %" PRIx64, server_stream_id);
}

/**
 * @brief Delete the information about server stream id.
 * @param[in] (port_type) Client port type when opened.
 * @param[in] (port_id) Client port id when opened.
 */
void ClientMessenger::DeleteServerStreamId(
    const std::string& port_type, int32_t port_id) {
  osal::OSLockMutex(mutex_server_stream_id_);
  ServerStreamIdList::iterator itr = server_stream_ids_.begin();
  ServerStreamIdList::const_iterator end = server_stream_ids_.end();
  for (; itr != end; ++itr) {
    if ((itr->port_type == port_type) && (itr->port_id == port_id)) {
      server_stream_ids_.erase(itr);
      break;
    }
  }
  osal::OSUnlockMutex(mutex_server_stream_id_);
}

/**
 * @brief Clear server stream id.
 */
void ClientMessenger::ClearServerStreamId() {
  osal::OSLockMutex(mutex_server_stream_id_);
  server_stream_ids_.clear();
  osal::OSUnlockMutex(mutex_server_stream_id_);
}

/**
 * @brief Lock for Client component.
 */
void ClientMessenger::LockComponent() {
  osal::OSLockMutex(mutex_component_);
}

/**
 * @brief Unlock for Client component.
 */
void ClientMessenger::UnlockComponent() {
  osal::OSUnlockMutex(mutex_component_);
}

/**
 * @brief Constructor.
 */
ClientMessenger::ClientMessenger()
    : connection_(), recv_thread_(), is_connected_(), child_(), request_id_(),
      frame_callback_(), frame_callback_arg_(),
      event_callback_(), event_callback_arg_() {
  mutex_waiting_ = new util::Mutex();
  osal::OSCreateMutex(&mutex_request_id_);
  osal::OSCreateMutex(&mutex_server_stream_id_);
  osal::OSCreateMutex(&mutex_component_);
}

/**
 * @brief Destructor.
 */
ClientMessenger::~ClientMessenger() {
  Stop();

  frame_callback_ = NULL;
  event_callback_ = NULL;

  delete mutex_waiting_;
  mutex_waiting_ = NULL;
  osal::OSDestroyMutex(mutex_request_id_);
  mutex_request_id_ = NULL;
  osal::OSDestroyMutex(mutex_server_stream_id_);
  mutex_server_stream_id_ = NULL;
  osal::OSDestroyMutex(mutex_component_);
  mutex_component_ = NULL;
}

}   // namespace senscord
