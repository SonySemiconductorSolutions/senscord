/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "component_client.h"
#include <inttypes.h>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <limits>
#include "senscord/osal_inttypes.h"
#include "senscord/connection_types.h"
#include "senscord/serialize.h"
#include "senscord/develop/property_types_private.h"
#include "senscord/develop/common_types.h"
#include "senscord/develop/client_messenger.h"
#include "senscord/develop/client_instance_utils.h"

#include "./autolock.h"
#include "./client_log.h"
#include "./client_messenger_manager.h"
#include "./client_property_accessor.h"

namespace client {

// component argument names.
// kArgumentNamePortNum : port_num is declared at property_types_private.h
static const char kArgumentNameThreading[] = "threading";     // want

// component argument values.
static const char kArgumentValueSerial[] = "serial";
static const char kArgumentValueParallel[] = "parallel";

// max port number.
static const uint32_t kDefaultPortNum = 5;
static const uint32_t kMaxPortNum = 256;

// reply timeout nanoseconds.
static const uint64_t kNsecPerMsec = 1000000;
static const uint64_t kDefaultTimeout = 30000ULL * kNsecPerMsec;  // 30,000 ms

// typedefs
typedef std::map<std::string, std::string>::const_iterator
  ArgumentConstIterator;

/**
 * @brief Create component instance.
 * @return Created component instance. In case of failure, it returns NULL.
 */
extern "C" void* CreateComponent() {
  return new ClientComponent();
}

/**
 * @brief Destroy component instance.
 * @param[in] component　 Instance created in CreateComponent().
 */
extern "C" void DestroyComponent(void* component) {
  delete reinterpret_cast<ClientComponent*>(component);
}

/**
 * @brief The callback for port sending message arrived.
 * @param[in] (msg) The message of arrived frame.
 * @param[in] (arg) The private data as component pointer.
 */
static void CallbackPortSendingMsgArrived(
    const std::string& port_type, int32_t port_id,
    senscord::Message* msg, void* arg) {
  if (arg) {
    ClientComponent* client = reinterpret_cast<ClientComponent*>(arg);
    senscord::Status status = client->PushPortSendingsMessage(
        port_type, port_id, msg);
    SENSCORD_STATUS_TRACE(status);
    if (!status.ok()) {
      SENSCORD_CLIENT_LOG_WARNING(status.ToString().c_str());
    }
  }
}

/**
 * @brief The callback on LockProperty called.
 * @param[in] (port) The port of component.
 * @param[in] (args) The arguments of callback.
 * @param[in] (private_data) The value by callback registered.
 */
static senscord::Status CallbackLockProperty(
    senscord::ComponentPort* port,
    const senscord::ComponentPort::LockPropertyArguments& args,
    void* private_data) {
  if (private_data) {
    ClientComponent* client = reinterpret_cast<ClientComponent*>(private_data);
    senscord::Status status = client->LockProperty(port, args);
    return SENSCORD_STATUS_TRACE(status);
  }
  return SENSCORD_STATUS_FAIL("client",
      senscord::Status::kCauseInvalidOperation, "no client");
}

/**
 * @brief The callback on UnlockProperty called.
 * @param[in] (port) The port of component.
 * @param[in] (lock_resource) Lock resource.
 * @param[in] (private_data) The value by callback registered.
 */
static senscord::Status CallbackUnlockProperty(
    senscord::ComponentPort* port,
    senscord::PropertyLockResource* lock_resource,
    void* private_data) {
  if (private_data) {
    ClientComponent* client = reinterpret_cast<ClientComponent*>(private_data);
    senscord::Status status = client->UnlockProperty(port, lock_resource);
    return SENSCORD_STATUS_TRACE(status);
  }
  return SENSCORD_STATUS_FAIL("client",
      senscord::Status::kCauseInvalidOperation, "no client");
}

/**
 * Initialize this component.
 */
senscord::Status ClientComponent::InitComponent(
    senscord::Core* core,
    senscord::ComponentPortManager* port_manager,
    const senscord::ComponentArgument& args) {
  instance_name_ = args.instance_name;
  port_manager_ = port_manager;

  if (args.allocators.empty()) {
    ExitComponent();
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "no allocator");
  }

  // create allocator list
  {
    std::map<std::string, senscord::MemoryAllocator*>::const_iterator itr =
        args.allocators.begin();
    std::map<std::string, senscord::MemoryAllocator*>::const_iterator end =
        args.allocators.end();
    for (; itr != end; ++itr) {
      allocators_.push_back(itr->second);
    }
    alloc_manager_.Init(allocators_);
  }

#if 0   // for debug
  {
    // print allocator
    typedef std::vector<senscord::MemoryAllocator*> AllocatorList;
    AllocatorList::const_iterator itr = allocators_.begin();
    AllocatorList::const_iterator end = allocators_.end();
    for (; itr != end; ++itr) {
      SENSCORD_CLIENT_LOG_INFO("allocator[%s]: %p",
          (*itr)->GetKey().c_str(), (*itr));
    }
  }
#endif

  // parse the component arguments
  senscord::Status status = AnalyzePortNum(args);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    ExitComponent();
    return status;
  }

  status = senscord::ClientInstanceUtility::GetConnectionAddress(
      args.arguments, &address_primary_, &address_secondary_);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    ExitComponent();
    return status;
  }

  status = AnalyzeThreading(args);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    ExitComponent();
    return status;
  }

  status = senscord::ClientInstanceUtility::GetConnectionType(
      args.arguments, &connection_mode_);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    ExitComponent();
    return status;
  }

  senscord::ClientInstanceUtility::GetConnectionReplyTimeout(
      args.arguments, &reply_timeout_nsec_);

  // print initial settings.
  SENSCORD_CLIENT_LOG_INFO("[client] InitComponent: %s",
      instance_name_.c_str());
  SENSCORD_CLIENT_LOG_INFO("  - threading  : %d (%s)",
      threading_, GetThreadingString());
  SENSCORD_CLIENT_LOG_INFO("  - port_num   : %" PRIu32, port_num_);
  SENSCORD_CLIENT_LOG_INFO("  - connection : %s", connection_mode_.c_str());
  SENSCORD_CLIENT_LOG_INFO("  - address primary   : %s",
                           address_primary_.c_str());
  SENSCORD_CLIENT_LOG_INFO("  - address secondary : %s",
                           address_secondary_.c_str());
  SENSCORD_CLIENT_LOG_INFO("  - reply_timeout_msec: %" PRIu64,
                           reply_timeout_nsec_ / kNsecPerMsec);

  // setup messengers.
  status = CreateMessengerManager();
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    ExitComponent();
    return status;
  }

  // open ports
  for (uint32_t port_id = 0; port_id < port_num_; ++port_id) {
    port_manager->CreatePort(senscord::kPortTypeClient, port_id, NULL);
  }

  return senscord::Status::OK();
}

/**
 * Exit this component.
 */
senscord::Status ClientComponent::ExitComponent() {
  // stop messenger
  if (msg_manager_) {
    for (uint32_t port_id = 0; port_id < port_num_; ++port_id) {
      msg_manager_->RemoveMessenger(port_id);
    }
    delete msg_manager_;
    msg_manager_ = NULL;
  }

  // remove all property accessors
  {
    senscord::osal::OSLockMutex(mutex_port_property_key_map_);
    PortPropertyKeyMap::const_iterator itr = port_property_key_map_.begin();
    PortPropertyKeyMap::const_iterator end = port_property_key_map_.end();
    for (; itr != end; ++itr) {
      UnregisterPortProperties(itr->first, itr->second);
      delete itr->second;
    }
    port_property_key_map_.clear();
    senscord::osal::OSUnlockMutex(mutex_port_property_key_map_);
  }

  alloc_manager_.Exit();

  // remove ports
  port_manager_->DestroyAllPort();

  allocators_.clear();
  port_manager_ = NULL;
  port_num_ = kDefaultPortNum;
  instance_name_.clear();
  return senscord::Status::OK();
}

/**
 * Open the port.
 */
senscord::Status ClientComponent::OpenPort(
    const std::string& port_type,
    int32_t port_id,
    const senscord::ComponentPortArgument& args) {
  SENSCORD_CLIENT_LOG_DEBUG("open port: %s (%s.%" PRId32 ")",
      args.stream_key.c_str(), port_type.c_str(), port_id);

  // create and start messenger
  senscord::ClientMessenger* messenger = NULL;
  senscord::Status status = CreateMessenger(port_id, &messenger);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    return status;
  }

  // message data payload
  senscord::MessageDataOpenRequest msg_data = {};
  msg_data.stream_key = args.stream_key;
  msg_data.arguments = args.arguments;

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeOpen, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: open port: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: open port: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataOpenReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataOpenReply*>(
            reply->data);

    uint64_t server_stream_id = reply->header.server_stream_id;

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    if (status.ok()) {
      // register callbacks
      senscord::ComponentPort* port = GetPort(port_type, port_id);
      if (port) {
        port->RegisterLockPropertyCallback(CallbackLockProperty, this);
        port->RegisterUnlockPropertyCallback(CallbackUnlockProperty, this);
      } else {
        status = SENSCORD_STATUS_FAIL("client",
            senscord::Status::kCauseInvalidOperation,
            "no existed port");
      }
    }
    if (status.ok()) {
      // add server info
      messenger->AddServerStreamId(port_type, port_id, server_stream_id);
      SENSCORD_CLIENT_LOG_DEBUG("%s: open port: server stream id: %" PRIx64,
          instance_name_.c_str(), server_stream_id);

      // register property key list
      status = RegisterProperties(port_type, port_id,
          reply_data.property_key_list);
      SENSCORD_STATUS_TRACE(status);
    }
    if (status.ok()) {
      status = StartPortSendings(port_type, port_id);
      SENSCORD_STATUS_TRACE(status);
    }

    // release reply
    messenger->ReleaseCommandReply(reply);

    if (status.ok()) {
      status = messenger->MakeSecondaryConnection(
          port_type, port_id, server_stream_id, reply_timeout_nsec_);
      SENSCORD_STATUS_TRACE(status);
    }
    if (!status.ok()) {
      StopPortSendings(port_type, port_id);
    }
  }

  if (!status.ok()) {
    // cancel
    msg_manager_->RemoveMessenger(port_id);
  }
  return status;
}

/**
 * Close the port.
 */
senscord::Status ClientComponent::ClosePort(
    const std::string& port_type, int32_t port_id) {
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  // stop sending to port.
  StopPortSendings(port_type, port_id);

  // create message data
  senscord::MessageDataCloseRequest msg_data = {};

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeClose, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: close port: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: close port: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataCloseReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataCloseReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  } else {
    if (!messenger->IsConnected()) {
      // If it is disconnected, it returns OK to release the resource.
      SENSCORD_CLIENT_LOG_WARNING(
          "%s: close port: disconnected: req_id=%" PRIu64 ", %s",
          instance_name_.c_str(), msg.header.request_id,
          status.ToString().c_str());
      status = senscord::Status::OK();
    }
  }

  if (status.ok()) {
    // delete server stream id.
    messenger->DeleteServerStreamId(port_type, port_id);
    senscord::Status status2 = UnregisterProperties(port_type, port_id);
    if (!status2.ok()) {
      SENSCORD_CLIENT_LOG_WARNING(
          "%s: close port: UnregisterProperties: %s",
          instance_name_.c_str(), status2.ToString().c_str());
    }
    DeletePortEvents(port_id);
    RemovePortLockResources(port_id);

    // delete
    msg_manager_->RemoveMessenger(port_id);

    // close mapping
    alloc_manager_.Close(port_id);
  }

  return status;
}

/**
 * Start the port.
 */
senscord::Status ClientComponent::StartPort(
    const std::string& port_type, int32_t port_id) {
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  messenger->LockComponent();

  // open mapping
  senscord::Status status;

  status = alloc_manager_.Open(port_id);
  if (!status.ok()) {
    messenger->UnlockComponent();
    return SENSCORD_STATUS_TRACE(status);
  }

  status = frame_manager_->Start(port_id);
  if (!status.ok()) {
    alloc_manager_.Close(port_id);
    messenger->UnlockComponent();
    return SENSCORD_STATUS_TRACE(status);
  }

  // create message data
  senscord::MessageDataStartRequest msg_data = {};

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeStart, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: start port: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: start port: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataStartReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataStartReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  }

  if (!status.ok()) {
    // OnReleaseAllFrames() function is called.
    frame_manager_->Stop(port_id);
  }

  messenger->UnlockComponent();

  return status;
}

/**
 * Stop the cport.
 */
senscord::Status ClientComponent::StopPort(
    const std::string& port_type, int32_t port_id) {
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  // create message data
  senscord::MessageDataStopRequest msg_data = {};

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeStop, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: stop port: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: stop port: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataStopReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataStopReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  } else {
    if (!messenger->IsConnected()) {
      // If it is disconnected, it returns OK to release the resource.
      SENSCORD_CLIENT_LOG_WARNING(
          "%s: stop port: disconnected: req_id=%" PRIu64 ", %s",
          instance_name_.c_str(), msg.header.request_id,
          status.ToString().c_str());
      status = senscord::Status::OK();
    }
  }

  if (status.ok()) {
    status = frame_manager_->Stop(port_id);
    SENSCORD_STATUS_TRACE(status);
  }

  return status;
}

/**
 * @brief Method for the port sending thread.
 * @param[in] (arg) The elements for port.
 * @return Don't care.
 */
static senscord::osal::OSThreadResult ThreadPortSending(void* arg) {
  ClientComponent::PortSendingElements* elements =
      reinterpret_cast<ClientComponent::PortSendingElements*>(arg);
  if (elements != NULL) {
    elements->component->MonitorMessages(elements);
  }
  return 0;
}

/**
 * @brief Monitor the port sending messages from server.
 * @param[in] (elements) The elements for port.
 */
void ClientComponent::MonitorMessages(PortSendingElements* elements) {
  SENSCORD_CLIENT_LOG_DEBUG("start MonitorMessages: %s %s.%" PRId32,
      instance_name_.c_str(), elements->port_type.c_str(), elements->port_id);

  senscord::ClientMessenger* messenger =
      msg_manager_->GetMessenger(elements->port_id);
  if (messenger == NULL) {
    SENSCORD_CLIENT_LOG_WARNING("MonitorMessages: unknown port id: %" PRId32,
        elements->port_id);
    return;
  }

  senscord::osal::OSLockMutex(elements->mutex);
  while (!elements->end_flg) {
    std::vector<senscord::Message*>::iterator itr = elements->messages.begin();
    if (itr != elements->messages.end()) {
      // message received and dequeue
      senscord::Message* msg = (*itr);
      elements->messages.erase(itr);
      senscord::osal::OSUnlockMutex(elements->mutex);

      // sending message
      switch (msg->header.type) {
        case senscord::kMessageTypeSendFrame:
          ArrivedFrame(messenger, elements->port_type, elements->port_id,
                       *msg);
          break;

        case senscord::kMessageTypeSendEvent:
          ArrivedEvent(elements->port_type, elements->port_id, *msg);
          break;

        default:
          SENSCORD_CLIENT_LOG_WARNING("%s: unknown message type: %d",
              instance_name_.c_str(), msg->header.type);
          break;
      }
      messenger->ReleaseCommandReply(msg);

      senscord::osal::OSLockMutex(elements->mutex);
    } else {
      // wait next message
      senscord::osal::OSWaitCond(elements->cond, elements->mutex);
    }
  }
  SENSCORD_CLIENT_LOG_DEBUG("stop MonitorMessages: %s %s.%" PRId32,
      instance_name_.c_str(), elements->port_type.c_str(), elements->port_id);

  // clear all messages (dropped)
  while (!elements->messages.empty()) {
    std::vector<senscord::Message*>::iterator itr = elements->messages.begin();
    messenger->ReleaseCommandReply(*itr);
    elements->messages.erase(itr);
  }
  senscord::osal::OSUnlockMutex(elements->mutex);
}

/**
 * @brief Push the message for port seinding.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (msg) Message from server.
 * @return Status object.
 */
senscord::Status ClientComponent::PushPortSendingsMessage(
    const std::string& port_type, int32_t port_id, senscord::Message* msg) {
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32
        " (memory leak)", port_id);
  }

  senscord::Status status;
  senscord::osal::OSLockMutex(mutex_port_sendings_);
  PortSendingElemMap::iterator itr = port_sendings_.find(port_id);
  if (itr != port_sendings_.end()) {
    // push and notify to monitor thread.
    PortSendingElements* elements = itr->second;
    senscord::osal::OSLockMutex(elements->mutex);
    if (elements->end_flg == false) {
      elements->messages.push_back(msg);
      senscord::osal::OSSignalCond(elements->cond);
    } else {
      messenger->ReleaseCommandReply(msg);
    }
    senscord::osal::OSUnlockMutex(elements->mutex);
  } else {
    messenger->ReleaseCommandReply(msg);
    status = SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }
  senscord::osal::OSUnlockMutex(mutex_port_sendings_);
  return status;
}

/**
 * @brief Start-up to send to the component port with server messages.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status ClientComponent::StartPortSendings(
    const std::string& port_type, int32_t port_id) {
  // create the elements for monitoring.
  PortSendingElements* elements = new PortSendingElements();
  elements->component = this;
  elements->port_type = port_type;
  elements->port_id = port_id;
  elements->end_flg = false;
  elements->messages.clear();
  senscord::osal::OSCreateMutex(&elements->mutex);
  senscord::osal::OSCreateCond(&elements->cond);

  // start threading
  int32_t ret = senscord::osal::OSCreateThread(
      &elements->thread, ThreadPortSending, elements, NULL);
  if (ret != 0) {
    senscord::osal::OSDestroyCond(elements->cond);
    senscord::osal::OSDestroyMutex(elements->mutex);
    delete elements;
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseAborted, "failed to create thread");
  }

  senscord::osal::OSLockMutex(mutex_port_sendings_);
  port_sendings_.insert(std::make_pair(port_id, elements));
  senscord::osal::OSUnlockMutex(mutex_port_sendings_);
  return senscord::Status::OK();
}

/**
 * @brief End to send to the component port with server messages.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 */
void ClientComponent::StopPortSendings(
    const std::string& port_type, int32_t port_id) {
  // remove the elements from map
  PortSendingElements* elements = NULL;
  senscord::osal::OSLockMutex(mutex_port_sendings_);
  PortSendingElemMap::iterator itr = port_sendings_.find(port_id);
  if (itr != port_sendings_.end()) {
    elements = itr->second;
    port_sendings_.erase(itr);
  }
  senscord::osal::OSUnlockMutex(mutex_port_sendings_);

  if (elements != NULL) {
    // wakeup and stop the monitoring thread.
    senscord::osal::OSLockMutex(elements->mutex);
    elements->end_flg = true;
    senscord::osal::OSSignalCond(elements->cond);
    senscord::osal::OSUnlockMutex(elements->mutex);

    // wait to finish and release resource.
    senscord::osal::OSJoinThread(elements->thread, NULL);

    senscord::osal::OSDestroyCond(elements->cond);
    senscord::osal::OSDestroyMutex(elements->mutex);
    delete elements;
  }
}

/**
 * @brief The processing for arrived multiple frames.
 * @param[in] (messenger) The messenger for sending reply.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (msg) The message of arrived frame.
 */
void ClientComponent::ArrivedFrame(
    senscord::ClientMessenger* messenger,
    const std::string& port_type, int32_t port_id,
    const senscord::Message& msg) {
  SENSCORD_CLIENT_LOG_DEBUG("[client] frame arrived");
  if (msg.data == NULL) {
    SENSCORD_CLIENT_LOG_WARNING("[client] msg.data is null");
    return;
  }

  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    SENSCORD_CLIENT_LOG_WARNING("[client] no existed port: %s.%" PRId32,
        port_type.c_str(), port_id);
    return;
  }

  messenger->LockComponent();

  // cast message data.
  const senscord::MessageDataSendFrame& msg_data =
      *reinterpret_cast<const senscord::MessageDataSendFrame*>(msg.data);

  std::vector<uint64_t> reply_frames;
  std::vector<senscord::MessageDataFrameLocalMemory>::const_iterator itr =
      msg_data.frames.begin();
  std::vector<senscord::MessageDataFrameLocalMemory>::const_iterator end =
      msg_data.frames.end();

  bool update_checked = false;
  while (itr != end) {
    std::vector<senscord::FrameInfo> frames;
    senscord::Status status;

    for (; itr != end; ++itr) {
      const senscord::MessageDataFrameLocalMemory& src_data = *itr;
      // update check
      if (update_checked || IsUpdatedFrameProperty(src_data)) {
        if (!frames.empty()) {
          update_checked = true;
          break;  // send
        }
        update_checked = false;

        // update properties.
        status = UpdateFrameProperties(port, src_data);
        if (!status.ok()) {
          SENSCORD_STATUS_TRACE(status);
          SENSCORD_CLIENT_LOG_WARNING("[client] %s", status.ToString().c_str());
          // continue processing.
        }
#if 0   // for debug
        SENSCORD_CLIENT_LOG_DEBUG(
            "[client] UpdateFrameProperties: seq_num=%" PRIu64,
            src_data.sequence_number);
#endif
      }

      // check the need to reply
      bool reply = IsReplyToSendFrame(src_data);
      if (reply) {
        reply_frames.push_back(src_data.sequence_number);
      }

      frames.push_back(senscord::FrameInfo());
      senscord::FrameInfo& frameinfo = *frames.rbegin();

      // Add to the management list
      status = frame_manager_->AddFrame(
          port_id, src_data.sequence_number);
      SENSCORD_STATUS_TRACE(status);
      if (status.cause() == senscord::Status::kCauseAlreadyExists) {
        // already sent frame, ignore process
        SENSCORD_CLIENT_LOG_WARNING(
            "[client] already sent frame, seqnum=%" PRIu64,
            src_data.sequence_number);
        frames.pop_back();
        continue;
      }
      // create frameinfo
      if (status.ok()) {
        status = CreateFrameInfo(port_id, &frameinfo, src_data);
        SENSCORD_STATUS_TRACE(status);
      }

      // Update user data
      if (status.ok()) {
        senscord::FrameUserData user_data = {};
        if (!src_data.user_data.empty()) {
          user_data.data_size = src_data.user_data.size();
          user_data.data_address =
              reinterpret_cast<uintptr_t>(&src_data.user_data[0]);
        }
        status = port->SetUserData(user_data);
        SENSCORD_STATUS_TRACE(status);
      }

      if (!status.ok()) {
        SENSCORD_CLIENT_LOG_WARNING("[client] %s", status.ToString().c_str());
        SendEventFrameDropped(port, src_data.sequence_number);
        // free allocated channels
        frameinfo.sequence_number = src_data.sequence_number;
        senscord::Status release_status =
            ReleasePortFrameCore(port_type, port_id, frameinfo, NULL, !reply);
        if (!release_status.ok()) {
          SENSCORD_CLIENT_LOG_WARNING("[client] %s",
              release_status.ToString().c_str());
        }
        frames.pop_back();
        continue;
      }
    }

    if (!frames.empty()) {
      // send multiple frames to stream(s).
      std::vector<uint64_t> dropped;
      status = port->SendFrames(frames, &dropped);
      if (!status.ok()) {
        SENSCORD_STATUS_TRACE(status);
        SENSCORD_CLIENT_LOG_WARNING("[client] %s", status.ToString().c_str());
        // release the dropped frames.
        ReleaseFrames(port_type, port_id, frames, dropped);
        // TODO: If the server is a different device and it
        // is a shared memory allocator, two ReleaseFrame requests will be sent.
        // However, there is no leak or double free, so it is not a big problem.
      }
    }
  }

  if (!reply_frames.empty()) {
    // create reply message.
    senscord::MessageDataSendFrameReply reply = {};
    reply.sequence_numbers.swap(reply_frames);
    senscord::Message reply_msg = {};
    reply_msg.header = msg.header;
    reply_msg.header.type = senscord::kMessageTypeReply;
    reply_msg.data = &reply;

    // send reply to server.
    senscord::Status status = messenger->SendCommandReply(reply_msg);
    SENSCORD_STATUS_TRACE(status);
    if (!status.ok()) {
      SENSCORD_CLIENT_LOG_WARNING("[client] failed to send frame reply: %s",
          status.ToString().c_str());
    }
  }

  messenger->UnlockComponent();
}

/**
 * @brief The processing for arrived event.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (msg) The message of arrived event.
 */
void ClientComponent::ArrivedEvent(
    const std::string& port_type, int32_t port_id,
    const senscord::Message& msg) {
  SENSCORD_CLIENT_LOG_DEBUG("[client] event arrived");
  if (msg.data == NULL) {
    SENSCORD_CLIENT_LOG_WARNING("[client] msg.data is null");
    return;
  }

  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    SENSCORD_CLIENT_LOG_WARNING("[client] no existed port: %s.%" PRId32,
        port_type.c_str(), port_id);
    return;
  }

  // cast messgae data
  const senscord::MessageDataSendEvent& msg_data =
      *reinterpret_cast<const senscord::MessageDataSendEvent*>(msg.data);

  // send event
  senscord::Status status = port->SendEvent(
      msg_data.event_type, msg_data.args);
  if (!status.ok()) {
    SENSCORD_STATUS_TRACE(status);
    SENSCORD_CLIENT_LOG_WARNING("[client] %s", status.ToString().c_str());
  }
}

/**
 * @brief Release frames that failed to be sent.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frames) List of frame information to send.
 * @param[in] (dropped_frames) List of sequence numbers of dropped frames.
 */
void ClientComponent::ReleaseFrames(
    const std::string& port_type, int32_t port_id,
    const std::vector<senscord::FrameInfo>& frames,
    const std::vector<uint64_t>& dropped_frames) {
  std::vector<uint64_t>::const_iterator drop_begin = dropped_frames.begin();
  std::vector<uint64_t>::const_iterator drop_end = dropped_frames.end();
  std::vector<senscord::FrameInfo>::const_iterator frame_itr = frames.begin();
  std::vector<senscord::FrameInfo>::const_iterator frame_end = frames.end();
  for (; frame_itr != frame_end; ++frame_itr) {
    std::vector<uint64_t>::const_iterator drop_itr = std::find(
        drop_begin, drop_end, frame_itr->sequence_number);
    if (drop_itr != drop_end) {
      ReleasePortFrame(port_type, port_id, *frame_itr, NULL);
    }
  }
}

/**
 * @brief Create the frame info for SendFrame.
 * @param[out] (dest) The destination of FrameInfo.
 * @param[in] (src) The frame message from the server.
 * @return Status object.
 */
senscord::Status ClientComponent::CreateFrameInfo(
    int32_t port_id, senscord::FrameInfo* dest,
    const senscord::MessageDataFrameLocalMemory& src) {
  dest->sequence_number = src.sequence_number;
  dest->sent_time = src.sent_time;
  dest->channels.resize(src.channels.size());

  typedef std::vector<senscord::MessageDataChannelLocalMemory> SrcChannel;
  SrcChannel::const_iterator itr = src.channels.begin();
  SrcChannel::const_iterator end = src.channels.end();
  for (size_t index = 0; itr != end; ++itr, ++index) {
    // create channel raw data.
    senscord::ChannelRawData& channel = dest->channels[index];
    channel.channel_id = itr->channel_id;
    channel.data_type = itr->rawdata_type;
    channel.captured_timestamp = itr->timestamp;

    const std::vector<uint8_t>& rawdata = itr->rawdata_info.rawdata;
    if (rawdata.size() > 0) {
      // mapping memory
      senscord::MemoryContained contained = {};
      senscord::Status status = alloc_manager_.Mapping(port_id,
          itr->allocator_key, rawdata, &contained);
      if (!status.ok()) {
        return SENSCORD_STATUS_TRACE(status);
      }
      channel.data_memory = contained.memory;
      channel.data_size = contained.size;
      channel.data_offset = contained.offset;

      // copy to memory
      if (itr->rawdata_info.delivering_mode == senscord::kDeliverAllData) {
        senscord::osal::OSMemcpy(
            reinterpret_cast<void*>(channel.data_memory->GetAddress()),
            channel.data_memory->GetSize(), &rawdata[0], rawdata.size());
      }
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Whether to reply to SendFrame message.
 * @param[in] (frame) The frame message from the server.
 * @return The need to reply.
 */
bool ClientComponent::IsReplyToSendFrame(
    const senscord::MessageDataFrameLocalMemory& frame) const {
  for (std::vector<senscord::MessageDataChannelLocalMemory>::const_iterator
      itr = frame.channels.begin(), end = frame.channels.end();
      itr != end; ++itr) {
    if (itr->rawdata_info.delivering_mode != senscord::kDeliverAllData) {
      // If mode other than AllData is included, do not reply.
      return false;
    }
  }
  return true;
}

/**
 * @brief Return whether the FrameProperty has been updated.
 * @param[in] (src) The frame message from the server.
 * @return whether the FrameProperty has been updated.
 */
bool ClientComponent::IsUpdatedFrameProperty(
    const senscord::MessageDataFrameLocalMemory& src) const {
  typedef std::vector<senscord::MessageDataChannelLocalMemory>
      SrcChannel;
  SrcChannel::const_iterator itr = src.channels.begin();
  SrcChannel::const_iterator end = src.channels.end();
  for (; itr != end; ++itr) {
    // check whether this channel has the updated property.
    if (itr->updated_property_keys.empty()) {
      continue;
    }

    // check each channel's property list.
    typedef std::vector<senscord::MessageDataProperty>
        SrcChannelProperty;
    SrcChannelProperty::const_iterator itr_prop = itr->properties.begin();
    SrcChannelProperty::const_iterator end_prop = itr->properties.end();
    for (; itr_prop != end_prop; ++itr_prop) {
      // check whether updated or not.
      std::vector<std::string>::const_iterator found =
          std::find(itr->updated_property_keys.begin(),
              itr->updated_property_keys.end(), itr_prop->key);
      if (found != itr->updated_property_keys.end()) {
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Update properties by the frame from the server.
 * @param[in] (port) The component port.
 * @param[in] (src) The frame message from the server.
 * @return Status object.
 */
senscord::Status ClientComponent::UpdateFrameProperties(
    senscord::ComponentPort* port,
    const senscord::MessageDataFrameLocalMemory& src) {
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "port is null");
  }

  typedef std::vector<senscord::MessageDataChannelLocalMemory>
      SrcChannel;
  SrcChannel::const_iterator itr = src.channels.begin();
  SrcChannel::const_iterator end = src.channels.end();
  for (; itr != end; ++itr) {
    // check whether this channel has the updated property.
    if (itr->updated_property_keys.size() == 0) {
      continue;
    }

    // check each channel's property list.
    typedef std::vector<senscord::MessageDataProperty>
        SrcChannelProperty;
    SrcChannelProperty::const_iterator itr_prop = itr->properties.begin();
    SrcChannelProperty::const_iterator end_prop = itr->properties.end();
    for (; itr_prop != end_prop; ++itr_prop) {
      // check whether updated or not.
      std::vector<std::string>::const_iterator found =
          std::find(itr->updated_property_keys.begin(),
              itr->updated_property_keys.end(), itr_prop->key);
      if (found == itr->updated_property_keys.end()) {
        continue;
      }

#if 0   // for debug
      SENSCORD_CLIENT_LOG_DEBUG("found updated property: %s, size=%" PRIdS,
          itr_prop->key.c_str(), itr_prop->property.data.size());
      for (size_t i = 0; i < itr_prop->property.data.size(); ++i) {
        senscord::osal::OSPrintf(" %02" PRIx8, itr_prop->property.data[i]);
      }
      senscord::osal::OSPrintf("\n");
#endif
      // updating
      senscord::Status status = port->UpdateFrameProperty(
          itr->channel_id, itr_prop->key, &itr_prop->property);
      if (!status.ok()) {
        return SENSCORD_STATUS_TRACE(status);
      }
    }
  }
  return senscord::Status::OK();
}

/**
 * Release the frame pushed from the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frameinfo) The frame information.
 * @param[in] (referenced_channel_ids) The list of referenced channel IDs.
 */
senscord::Status ClientComponent::ReleasePortFrame(
    const std::string& port_type,
    int32_t port_id,
    const senscord::FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids) {
  senscord::Status status = ReleasePortFrameCore(
      port_type, port_id, frameinfo, referenced_channel_ids, false);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * Release the frame pushed from the port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (frameinfo) The frame information.
 * @param[in] (referenced_channel_ids) The list of referenced channel IDs.
 * @param[in] (required_release_to_server) The flag to release to server.
 */
senscord::Status ClientComponent::ReleasePortFrameCore(
    const std::string& port_type,
    int32_t port_id,
    const senscord::FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids,
    bool required_release_to_server) {
  senscord::Status status;

  // free all allocated memory
  typedef std::vector<senscord::ChannelRawData>::const_iterator ConstIterator;
  ConstIterator itr = frameinfo.channels.begin();
  ConstIterator end = frameinfo.channels.end();
  for (; itr != end; ++itr) {
    if (itr->data_memory) {
      // check sharing
      // TODO: Send ReleaseFrame request only if the server
      // is on the same device and it is a shared memory allocator.
      required_release_to_server |=
          itr->data_memory->GetAllocator()->IsMemoryShared();

      // unmapping
      senscord::MemoryContained contained = {};
      contained.memory = itr->data_memory;
      contained.size = itr->data_size;
      contained.offset = itr->data_offset;
      senscord::Status status_unmap = alloc_manager_.Unmapping(
          port_id, contained);
      if (status.ok()) {
        status = status_unmap;
      }
    }
  }

  if (status.ok()) {
    frame_manager_->RemoveFrame(port_id, frameinfo.sequence_number);
  }

  // send to server
  if (status.ok() && required_release_to_server) {
    // get messenger
    senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
    if (messenger == NULL) {
      return SENSCORD_STATUS_FAIL("client",
          senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
          port_id);
    }

    // create message data
    senscord::MessageDataReleaseFrameRequest msg_data = {};
    msg_data.sequence_number = frameinfo.sequence_number;
    if (referenced_channel_ids != NULL) {
      msg_data.rawdata_accessed = !referenced_channel_ids->empty();
    }

    // create message
    senscord::Message msg = {};
    messenger->CreateRequestMessage(&msg, port_type, port_id,
        senscord::kMessageDataTypeReleaseFrame, &msg_data);

    senscord::Message* reply = NULL;

    // send request
    SENSCORD_CLIENT_LOG_DEBUG(
        "%s: release port frame: send request: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->SendCommandRequest(msg);
    SENSCORD_STATUS_TRACE(status);

    if (status.ok()) {
      // wait reply
      SENSCORD_CLIENT_LOG_DEBUG(
          "%s: release port frame: wait reply: req_id=%" PRIu64,
          instance_name_.c_str(), msg.header.request_id);
      status = messenger->WaitCommandReply(
          msg.header.request_id, reply_timeout_nsec_, &reply);
      SENSCORD_STATUS_TRACE(status);
    }

    if (status.ok()) {
      // cast reply payload
      const senscord::MessageDataReleaseFrameReply& reply_data =
          *reinterpret_cast<const senscord::MessageDataReleaseFrameReply*>(
              reply->data);

      // check return status.
      status = reply_data.status.Get();
      SENSCORD_STATUS_TRACE(status);

      messenger->ReleaseCommandReply(reply);
    } else {
      if (!messenger->IsConnected()) {
        // If it is disconnected, it returns OK to release the resource.
        SENSCORD_CLIENT_LOG_WARNING(
            "%s: release port frame: disconnected: req_id=%" PRIu64 ", %s",
            instance_name_.c_str(), msg.header.request_id,
            status.ToString().c_str());
        status = senscord::Status::OK();
      }
    }
  }
  return status;
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
senscord::Status ClientComponent::SetProperty(
    const std::string& port_type,
    int32_t port_id,
    const std::string& key,
    const void* serialized_property,
    size_t serialized_size) {
  // register event callback to client
  if (key == senscord::kRegisterEventPropertyKey) {
    senscord::Status status = RegisterEvent(port_type, port_id,
        serialized_property, serialized_size);
    return SENSCORD_STATUS_TRACE(status);
  } else if (key == senscord::kUnregisterEventPropertyKey) {
    senscord::Status status = UnregisterEvent(port_type, port_id,
        serialized_property, serialized_size);
    return SENSCORD_STATUS_TRACE(status);
  }
  // create message data
  senscord::MessageDataSetPropertyRequest msg_data = {};
  msg_data.key = key;
  msg_data.property.data.resize(serialized_size);
  if (serialized_size > 0) {
    senscord::osal::OSMemcpy(
        &msg_data.property.data[0],
        msg_data.property.data.size(),
        serialized_property,
        serialized_size);
  }

  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  senscord::Message* reply = NULL;

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeSetProperty, &msg_data);

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: set property: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: set property: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataSetPropertyReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataSetPropertyReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  }
#ifdef SENSCORD_PLAYER
  // updated property from client
  if (key == senscord::kPlayPropertyKey) {
    status = ReloadProperties(port_type, port_id, senscord::kPlayPropertyKey);
    SENSCORD_STATUS_TRACE(status);
  }
#endif  // SENSCORD_PLAYER
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
senscord::Status ClientComponent::GetProperty(
    const std::string& port_type,
    int32_t port_id,
    const std::string& key,
    const void* serialized_input_property,
    size_t serialized_input_size,
    void** serialized_property,
    size_t* serialized_size) {
  if ((serialized_property == NULL) || (serialized_size == NULL)) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "parameter is null");
  }

  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  // create message data
  senscord::MessageDataGetPropertyRequest msg_data = {};
  msg_data.key = key;
  msg_data.property.data.resize(serialized_input_size);
  if (serialized_input_size > 0) {
    if (serialized_input_property == NULL) {
      return SENSCORD_STATUS_FAIL("client",
          senscord::Status::kCauseInvalidArgument, "parameter is null");
    }
    senscord::osal::OSMemcpy(
        &msg_data.property.data[0],
        msg_data.property.data.size(),
        serialized_input_property,
        serialized_input_size);
  }

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeGetProperty, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: get property: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: get property: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataGetPropertyReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataGetPropertyReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // create new serialized
    if (status.ok()) {
      *serialized_size = reply_data.property.data.size();
      if (*serialized_size > 0) {
        *serialized_property = new uint8_t[*serialized_size]();
        senscord::osal::OSMemcpy(*serialized_property, *serialized_size,
            &reply_data.property.data[0], *serialized_size);
      } else {
        *serialized_property = NULL;
      }
    }

    // release reply
    messenger->ReleaseCommandReply(reply);
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
senscord::Status ClientComponent::ReleaseProperty(
    const std::string& key,
    void* serialized_property,
    size_t serialized_size) {
  if (serialized_size == 0) {
    // do nothing
    return senscord::Status::OK();
  }
  if (serialized_property == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "parameter is null");
  }
  delete [] reinterpret_cast<uint8_t*>(serialized_property);
  return senscord::Status::OK();
}

/**
 * @brief Register event callback.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (serialized_property) Serialized property address.
 * @param[in] (serialized_size) Serialized property size.
 * @return Status object.
 */
senscord::Status ClientComponent::RegisterEvent(
    const std::string& port_type,
    int32_t port_id,
    const void* serialized_property,
    size_t serialized_size) {
  if (serialized_property == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "parameter is null");
  }
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }
  // deserialize property
  senscord::serialize::Decoder decoder(serialized_property, serialized_size);
  senscord::RegisterEventProperty property;
  senscord::Status status = decoder.Pop(property);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // check event reference of port
  PortEvent* port_event = NULL;
  status = GetPortEventElement(port_id, property.event_type, &port_event);
  if (!status.ok()) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }
  if (port_event->referenced > 0) {
    ++port_event->referenced;
    SENSCORD_CLIENT_LOG_DEBUG(
        "already registerd id: %" PRId32 " referenced: %" PRIu32,
        port_id, port_event->referenced);
    return senscord::Status::OK();
  }

  // create message
  senscord::Message msg = {};
  senscord::MessageDataRegisterEventRequest msg_data = {};
  msg_data.event_type = property.event_type;
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeRegisterEvent, &msg_data);

  senscord::Message* reply = NULL;
  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: register event: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: register event: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataRegisterEventReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataRegisterEventReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  }

  if (status.ok()) {
    ++port_event->referenced;
  }
  return status;
}

/**
 * @brief Unregister event callback.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (serialized_property) Serialized property address.
 * @param[in] (serialized_size) Serialized property size.
 * @return Status object.
 */
senscord::Status ClientComponent::UnregisterEvent(
    const std::string& port_type,
    int32_t port_id,
    const void* serialized_property,
    size_t serialized_size) {
  if (serialized_property == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "parameter is null");
  }
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }
  // deserialize property
  senscord::serialize::Decoder decoder(serialized_property, serialized_size);
  senscord::RegisterEventProperty property;
  senscord::Status status = decoder.Pop(property);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // check event reference of port
  PortEvent* port_event = NULL;
  status = GetPortEventElement(port_id, property.event_type, &port_event);
  if (!status.ok()) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }
  --port_event->referenced;
  if (port_event->referenced > 0) {
    SENSCORD_CLIENT_LOG_DEBUG(
        "still have referrers id: %" PRId32 " referenced: %" PRIu32,
        port_id, port_event->referenced);
    return senscord::Status::OK();
  }

  // create message
  senscord::Message msg = {};
  senscord::MessageDataUnregisterEventRequest msg_data = {};
  msg_data.event_type = property.event_type;
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeUnregisterEvent, &msg_data);

  senscord::Message* reply = NULL;
  // send request
  SENSCORD_CLIENT_LOG_DEBUG(
      "%s: unregister event: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG(
        "%s: unregister event: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataStandardReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataStandardReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  }

  if (!status.ok()) {
    ++port_event->referenced;   // rollback
  }
  return status;
}

/**
 * @brief Get port event element.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (event_type) The type of event.
 * @param[out] (port_event) The port event element.
 * @return Status object.
 */
senscord::Status ClientComponent::GetPortEventElement(
    int32_t port_id, const std::string& event_type, PortEvent** port_event) {
  senscord::osal::OSLockMutex(mutex_port_event_map_);
  PortEvents* port_events = &port_event_map_[port_id];
  PortEvent* element = NULL;
  PortEvents::iterator itr = port_events->begin();
  PortEvents::iterator end = port_events->end();
  for (; itr != end; ++itr) {
    // search event
    if ((*itr)->event_type == event_type) {
      element = *itr;
      break;
    }
  }
  if (element == NULL) {
    // new register event type
    element = new PortEvent;
    element->event_type = event_type;
    element->referenced = 0;
    port_events->push_back(element);
  }
  *port_event = element;
  senscord::osal::OSUnlockMutex(mutex_port_event_map_);
  return senscord::Status::OK();
}

/**
 * @brief Delete port event.
 * @param[in] (port_id) The ID of port type.
 */
void ClientComponent::DeletePortEvents(int32_t port_id) {
  senscord::osal::OSLockMutex(mutex_port_event_map_);
  PortEvents* port_events = &port_event_map_[port_id];
  while (!port_events->empty()) {
    PortEvent* port_event = port_events->back();
    port_events->pop_back();
    if (port_event != NULL) {
      delete port_event;
      port_event = NULL;
    }
  }
  port_event_map_.erase(port_id);
  senscord::osal::OSUnlockMutex(mutex_port_event_map_);
}

/**
 * @brief Send the frame dropped event.
 * @param[in] (port) The component port.
 * @param[in] (sequence_number) The sequence number that was dropped.
 */
void ClientComponent::SendEventFrameDropped(
    senscord::ComponentPort* port, uint64_t sequence_number) {
  senscord::EventArgument args;
  args.Set(senscord::kEventArgumentSequenceNumber, sequence_number);
  port->SendEvent(senscord::kEventFrameDropped, args);
}

/**
 * @brief Analyze the component arguments for port numbers.
 * @param[in] (args) The arguments of the component.
 * @return Status object.
 */
senscord::Status ClientComponent::AnalyzePortNum(
    const senscord::ComponentArgument& args) {
  ArgumentConstIterator itr =
      args.arguments.find(senscord::kArgumentNamePortNum);
  if (itr == args.arguments.end()) {
    // use default
    return senscord::Status::OK();
  }

  uint64_t port_num = 0;
  if (senscord::osal::OSStrtoull(itr->second.c_str(), NULL, 0, &port_num) < 0) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument,
        "%s is not number.", senscord::kArgumentNamePortNum);
  }
  if ((port_num == 0) || (port_num > kMaxPortNum)) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument,
        "%s is over range: %" PRIu64,
        senscord::kArgumentNamePortNum, port_num);
  }
  port_num_ = static_cast<uint32_t>(port_num);
  return senscord::Status::OK();
}

/**
 * @brief Analyze the component arguments for threading mode.
 * @param[in] (args) The arguments of the component.
 * @return Status object.
 */
senscord::Status ClientComponent::AnalyzeThreading(
    const senscord::ComponentArgument& args) {
  ArgumentConstIterator itr = args.arguments.find(kArgumentNameThreading);
  if (itr == args.arguments.end()) {
    // use default
    return senscord::Status::OK();
  }

  if (itr->second == kArgumentValueSerial) {
    threading_ = kThreadingSerial;
  } else if (itr->second == kArgumentValueParallel) {
    threading_ = kThreadingParallel;
  } else {
    // use default
    SENSCORD_CLIENT_LOG_WARNING(
        "%s=%s is invalid. use the default threading mode.",
        kArgumentNameThreading, itr->second.c_str());
  }
  return senscord::Status::OK();
}

/**
 * @brief Get property list to server.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[out] (property_list) Property list obtained from server.
 * @return Status object.
 */
senscord::Status ClientComponent::GetPropertyList(
    const std::string& port_type,
    int32_t port_id,
    std::vector<std::string>* property_list) {
  if (property_list == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "parameter is null");
  }
  // get messenger
  senscord::ClientMessenger* messenger = msg_manager_->GetMessenger(port_id);
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port_id);
  }

  // create message
  senscord::Message msg = {};
  senscord::MessageDataGetPropertyListRequest msg_data = {};
  messenger->CreateRequestMessage(&msg, port_type, port_id,
      senscord::kMessageDataTypeGetPropertyList, &msg_data);

  senscord::Message* reply = NULL;
  // send request
  SENSCORD_CLIENT_LOG_DEBUG(
      "%s: get property list: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG(
        "%s: get property list: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    senscord::MessageDataPropertyListReply& reply_data =
        *reinterpret_cast<senscord::MessageDataPropertyListReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    if (status.ok()) {
      property_list->swap(reply_data.property_list);
    }

    // release reply
    messenger->ReleaseCommandReply(reply);
  }

  return status;
}

/**
 * @brief Register the properties to created component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (key_list) The list of supported property keys.
 * @return Status object.
 */
senscord::Status ClientComponent::RegisterProperties(
    const std::string& port_type,
    int32_t port_id,
    const PropertyKeyList& key_list) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }
  // register to self map
  PropertyKeyList* new_key_list = new PropertyKeyList();
  senscord::Status status =
      RegisterPortProperties(port, key_list, new_key_list);
  if (status.ok()) {
    senscord::osal::OSLockMutex(mutex_port_property_key_map_);
    bool ret = port_property_key_map_.insert(
        std::make_pair(port, new_key_list)).second;
    senscord::osal::OSUnlockMutex(mutex_port_property_key_map_);
    if (!ret) {
      status = SENSCORD_STATUS_FAIL("client",
          senscord::Status::kCauseInvalidArgument,
          "port property has already been registered: %s, %" PRId32,
          port_type.c_str(), port_id);
    }
  }
  if (!status.ok()) {
    delete new_key_list;
  }
  return status;
}

/**
 * @brief Register the properties to component port.
 * @param[in] (port) The component port.
 * @param[in] (key_list) The list of supported property keys.
 * @param[out] (dst_key_list) The list of registerd property keys.
 * @return Status object.
 */
senscord::Status ClientComponent::RegisterPortProperties(
    senscord::ComponentPort* port,
    const PropertyKeyList& key_list,
    PropertyKeyList* dst_key_list) {
  PropertyKeyList tmp_key_list = key_list;
  // if the connection destination is client, it is already registered
  if (std::find(tmp_key_list.begin(), tmp_key_list.end(),
      senscord::kRegisterEventPropertyKey) == tmp_key_list.end()) {
    tmp_key_list.push_back(senscord::kRegisterEventPropertyKey);
  }
  if (std::find(tmp_key_list.begin(), tmp_key_list.end(),
      senscord::kUnregisterEventPropertyKey) == tmp_key_list.end()) {
    tmp_key_list.push_back(senscord::kUnregisterEventPropertyKey);
  }

  // register to port
  senscord::Status status;
  PropertyKeyList::const_iterator itr = tmp_key_list.begin();
  PropertyKeyList::const_iterator end = tmp_key_list.end();
  for (; itr != end; ++itr) {
    const std::string& key = (*itr);
    // remove the stream property
    if ((key != senscord::kStreamTypePropertyKey) &&
        (key != senscord::kStreamKeyPropertyKey) &&
        (key != senscord::kStreamStatePropertyKey) &&
        (key != senscord::kFrameBufferingPropertyKey) &&
        (key != senscord::kCurrentFrameNumPropertyKey) &&
        (key != senscord::kRecordPropertyKey) &&
        (key != senscord::kRecorderListPropertyKey)) {
      SENSCORD_CLIENT_LOG_DEBUG("[client] found property: %s", key.c_str());

      // register
      senscord::PropertyAccessor* accessor = new ClientPropertyAccessor(
          key, this, port->GetPortType(), port->GetPortId());
      status = port->RegisterPropertyAccessor(accessor);
      if (!status.ok()) {
        delete accessor;
        UnregisterPortProperties(port, dst_key_list);
        return SENSCORD_STATUS_TRACE(status);
      }
      dst_key_list->push_back(key);
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Unregister the properties from component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @return Status object.
 */
senscord::Status ClientComponent::UnregisterProperties(
    const std::string& port_type, int32_t port_id) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  senscord::Status status;

  // remove from map
  senscord::osal::OSLockMutex(mutex_port_property_key_map_);
  PortPropertyKeyMap::iterator itr = port_property_key_map_.find(port);
  if (itr == port_property_key_map_.end()) {
    status = SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound,
        "no registered properties: %s, %" PRId32, port_type.c_str(), port_id);
  }
  if (status.ok()) {
    PropertyKeyList* key_list = itr->second;
    port_property_key_map_.erase(itr);

    // unregister for all properties
    UnregisterPortProperties(port, key_list);
    delete key_list;
  }
  senscord::osal::OSUnlockMutex(mutex_port_property_key_map_);
  return status;
}

/**
 * @brief Unregister the properties from component port.
 * @param[in] (port) The component port.
 * @param[in] (key_list) The list of supported property keys.
 */
void ClientComponent::UnregisterPortProperties(
    senscord::ComponentPort* port, const PropertyKeyList* key_list) {
  if ((port) && (key_list)) {
    senscord::Status status;
    PropertyKeyList::const_iterator itr = key_list->begin();
    PropertyKeyList::const_iterator end = key_list->end();
    for (; itr != end; ++itr) {
      senscord::PropertyAccessor* accessor = NULL;
      status = port->UnregisterPropertyAccessor(*itr, &accessor);
      if (!status.ok()) {
        SENSCORD_CLIENT_LOG_WARNING(
            "%s: unregister port properties: %s",
            instance_name_.c_str(), status.ToString().c_str());
      }
      delete accessor;
      SENSCORD_CLIENT_LOG_DEBUG("[client] unregister property: %s",
          itr->c_str());
    }
  }
}

#ifdef SENSCORD_PLAYER
/**
 * @brief Reload the properties from to component port.
 * @param[in] (port_type) The type of port.
 * @param[in] (port_id) The ID of port type.
 * @param[in] (caller_property_key) Caller property key.
 * @return Status object.
 */
senscord::Status ClientComponent::ReloadProperties(
    const std::string& port_type,
    int32_t port_id,
    const std::string& caller_property_key) {
  // get port
  senscord::ComponentPort* port = port_manager_->GetPort(port_type, port_id);
  if (port == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument,
        "no existed port: %s, %" PRId32, port_type.c_str(), port_id);
  }

  // get property list from server
  PropertyKeyList new_key_list;
  senscord::Status status =
      GetPropertyList(port_type, port_id, &new_key_list);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // get port property list
  senscord::osal::OSLockMutex(mutex_port_property_key_map_);
  PortPropertyKeyMap::iterator result = port_property_key_map_.find(port);
  if (result == port_property_key_map_.end()) {
    status = SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound,
        "no registered properties: %s, %" PRId32, port_type.c_str(), port_id);
  }
  if (status.ok()) {
    PropertyKeyList* key_list = result->second;
    PropertyKeyList tmp_key_list = *key_list;

    // unregister all properties (excluded caller property)
    tmp_key_list.erase(
        std::remove(
            tmp_key_list.begin(), tmp_key_list.end(), caller_property_key),
            tmp_key_list.end());
    UnregisterPortProperties(port, &tmp_key_list);

    bool diff = tmp_key_list.size() != key_list->size();
    key_list->clear();
    if (diff) {
      key_list->push_back(caller_property_key);
    }

    // new register properties
    new_key_list.erase(
        std::remove(
            new_key_list.begin(), new_key_list.end(), caller_property_key),
            new_key_list.end());
    status = RegisterPortProperties(port, new_key_list, key_list);
    SENSCORD_STATUS_TRACE(status);
  }
  senscord::osal::OSUnlockMutex(mutex_port_property_key_map_);
  return status;
}
#endif  // SENSCORD_PLAYER

/**
 * @brief Lock the port properties.
 * @param[in] (port) The target port.
 * @param[in] (args) The arguments of callback.
 * @return Status object.
 */
senscord::Status ClientComponent::LockProperty(
    senscord::ComponentPort* port,
    const senscord::ComponentPort::LockPropertyArguments& args) {
  // create message data
  senscord::MessageDataLockPropertyRequest msg_data = {};
  msg_data.keys = args.keys;
  msg_data.timeout_msec = args.timeout_msec;

  // get messenger
  senscord::ClientMessenger* messenger =
      msg_manager_->GetMessenger(port->GetPortId());
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port->GetPortId());
  }

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port->GetPortType(),
      port->GetPortId(), senscord::kMessageDataTypeLockProperty, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG("%s: lock property: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG("%s: lock property: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    uint64_t timeout = 0;
    if (reply_timeout_nsec_ != 0 && args.timeout_msec >= 0 &&
        ((reply_timeout_nsec_ / kNsecPerMsec) + args.timeout_msec) <=
        (std::numeric_limits<uint64_t>::max() / kNsecPerMsec)) {
      timeout = reply_timeout_nsec_ +
          static_cast<uint64_t>(args.timeout_msec) * kNsecPerMsec;
    }
    status = messenger->WaitCommandReply(
        msg.header.request_id, timeout, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataLockPropertyReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataLockPropertyReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    if (status.ok()) {
      // set lock resource
      AutoLock lock(mutex_port_lock_resources_);
      PortLockResources& port_lock = port_lock_resources_[port->GetPortId()];
      port_lock[args.lock_resource].resource_id =
          reply_data.resource_id;
    }

    // release reply
    messenger->ReleaseCommandReply(reply);
  }

  return status;
}

/**
 * @brief Unlock the port properties.
 * @param[in] (port) The target port.
 * @param[in] (lock_resource) Lock resource.
 * @return Status object.
 */
senscord::Status ClientComponent::UnlockProperty(
    senscord::ComponentPort* port,
    senscord::PropertyLockResource* lock_resource) {
  // create message data
  senscord::MessageDataUnlockPropertyRequest msg_data = {};
  {
    AutoLock lock(mutex_port_lock_resources_);
    PortLockResources& port_lock = port_lock_resources_[port->GetPortId()];
    PortLockResources::iterator found = port_lock.find(lock_resource);
    if (found != port_lock.end()) {
      msg_data.resource_id = found->second.resource_id;
    }
  }

  // get messenger
  senscord::ClientMessenger* messenger =
      msg_manager_->GetMessenger(port->GetPortId());
  if (messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseNotFound, "unknown port id: %" PRId32,
        port->GetPortId());
  }

  // create message
  senscord::Message msg = {};
  messenger->CreateRequestMessage(&msg, port->GetPortType(),
      port->GetPortId(), senscord::kMessageDataTypeUnlockProperty, &msg_data);

  senscord::Message* reply = NULL;

  // send request
  SENSCORD_CLIENT_LOG_DEBUG(
      "%s: unlock property: send request: req_id=%" PRIu64,
      instance_name_.c_str(), msg.header.request_id);
  senscord::Status status = messenger->SendCommandRequest(msg);
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // wait reply
    SENSCORD_CLIENT_LOG_DEBUG(
        "%s: unlock property: wait reply: req_id=%" PRIu64,
        instance_name_.c_str(), msg.header.request_id);
    status = messenger->WaitCommandReply(
        msg.header.request_id, reply_timeout_nsec_, &reply);
    SENSCORD_STATUS_TRACE(status);
  }

  if (status.ok()) {
    // cast reply payload
    const senscord::MessageDataUnlockPropertyReply& reply_data =
        *reinterpret_cast<const senscord::MessageDataUnlockPropertyReply*>(
            reply->data);

    // check return status.
    status = reply_data.status.Get();
    SENSCORD_STATUS_TRACE(status);

    // release reply
    messenger->ReleaseCommandReply(reply);
  } else {
    if (!messenger->IsConnected()) {
      // If it is disconnected, it returns OK to release the resource.
      SENSCORD_CLIENT_LOG_WARNING(
          "%s: unlock property: disconnected: req_id=%" PRIu64 ", %s",
          instance_name_.c_str(), msg.header.request_id,
          status.ToString().c_str());
      status = senscord::Status::OK();
    }
  }

  // remove resource
  if (status.ok()) {
    AutoLock lock(mutex_port_lock_resources_);
    PortLockResources& port_lock = port_lock_resources_[port->GetPortId()];
    port_lock.erase(lock_resource);
  }

  return status;
}

/**
 * @brief Remove resource of lock property.
 * @param[in] (port_id) The ID of port.
 */
void ClientComponent::RemovePortLockResources(int32_t port_id) {
  AutoLock lock(mutex_port_lock_resources_);
  PortLockResources& port_lock = port_lock_resources_[port_id];
  port_lock.clear();
  port_lock_resources_.erase(port_id);
}

/**
 * @brief Release the all frames.
 *
 * This function is called when the following conditions:
 * - When stream stop is called when there is no frame being acquired.
 * - When all frames are released after stream stop.
 *
 * @param[in] (port_id) Port ID.
 */
void ClientComponent::OnReleaseAllFrames(int32_t port_id) {
  // close mapping
  senscord::Status status = alloc_manager_.Close(port_id);
  if (!status.ok()) {
    SENSCORD_CLIENT_LOG_WARNING(
        "%s: failed to close mapping: req_id=%" PRId32 ", %s",
        instance_name_.c_str(), port_id, status.ToString().c_str());
  }
}

/**
 * @brief Get the port address created.
 * @param[in] (type) The type of port.
 * @param[in] (id) The ID of port.
 * @return Component port.
 */
senscord::ComponentPort* ClientComponent::GetPort(
    const std::string& type, int32_t id) const {
  if (port_manager_) {
    return port_manager_->GetPort(type, id);
  }
  return NULL;
}

/**
 * @brief Create the manager of the messenger.
 * @return Status object.
 */
senscord::Status ClientComponent::CreateMessengerManager() {
  if (msg_manager_ == NULL) {
    switch (threading_) {
      case kThreadingSerial:
        msg_manager_ = new ClientMessengerManagerSerial();
        break;
      case kThreadingParallel:  /* FALLTHROUGH */
      default:
        msg_manager_ = new ClientMessengerManagerParallel();
        break;
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Create the manager on port.
 * @param[in] (port_id) The port id.
 * @param[out] (messenger) The created messenger.
 * @return Status object.
 */
senscord::Status ClientComponent::CreateMessenger(
    int32_t port_id, senscord::ClientMessenger** messenger) {
  *messenger = msg_manager_->CreateMessenger(port_id);
  if (*messenger == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseAborted,
        "failed to create messenger");
  }
  (*messenger)->RegisterFrameCallback(CallbackPortSendingMsgArrived, this);
  (*messenger)->RegisterEventCallback(CallbackPortSendingMsgArrived, this);
  senscord::Status status = (*messenger)->Start(
      connection_mode_, address_primary_, address_secondary_);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    msg_manager_->RemoveMessenger(port_id);
  }
  return status;
}

/**
 * @brief Get the string of the current threading mode.
 * @return the string of the current threading mode.
 */
const char* ClientComponent::GetThreadingString() const {
  switch (threading_) {
    case kThreadingSerial:
      return kArgumentValueSerial;
    case kThreadingParallel:  /* FALLTHROUGH */
    default:
      return kArgumentValueParallel;
  }
}

/**
 * @brief Constructor.
 */
ClientComponent::ClientComponent()
    : port_num_(kDefaultPortNum), instance_name_(),
      address_primary_(), address_secondary_(),
      threading_(), connection_mode_(),
      reply_timeout_nsec_(kDefaultTimeout),
      port_manager_(), msg_manager_(), frame_manager_() {
  frame_manager_ = new PortFrameManager(this);
  senscord::osal::OSCreateMutex(&mutex_port_property_key_map_);
  senscord::osal::OSCreateMutex(&mutex_port_sendings_);
  senscord::osal::OSCreateMutex(&mutex_port_event_map_);
  senscord::osal::OSCreateMutex(&mutex_port_lock_resources_);
}

/**
 * @brief Destructor.
 */
ClientComponent::~ClientComponent() {
  senscord::osal::OSDestroyMutex(mutex_port_property_key_map_);
  senscord::osal::OSDestroyMutex(mutex_port_sendings_);
  senscord::osal::OSDestroyMutex(mutex_port_event_map_);
  senscord::osal::OSDestroyMutex(mutex_port_lock_resources_);
  delete frame_manager_;
}

}   // namespace client
