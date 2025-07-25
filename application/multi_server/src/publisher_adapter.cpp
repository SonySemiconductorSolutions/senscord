/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "publisher_adapter.h"
#include "senscord/memory_allocator.h"
#include "server_log.h"

// private header
#include "allocator/memory_manager.h"

namespace {

static const char kServerNameLocalhost[] = "localhost";

/**
 * @brief The threading method for publisher adapter.
 * @param[in] (arg) The pointer as publisher adapter.
 * @return Don't care.
 */
static senscord::osal::OSThreadResult PublisherAdapterThread(void* arg) {
  if (arg) {
    senscord::server::PublisherAdapter* adapter =
        reinterpret_cast<senscord::server::PublisherAdapter*>(arg);
    adapter->Monitoring(senscord::server::PublisherAdapter::kMonitorStandard);
  }
  return static_cast<senscord::osal::OSThreadResult>(0);
}

/**
 * @brief The callback for release frame.
 * @param[in] (param) The publisher param.
 * @param[in] (frameinfo) Release frame info.
 */
static void CallbackReleaseFrame(
    const senscord::PublisherParam& param,
    const senscord::FrameInfo& frameinfo) {
  SENSCORD_SERVER_LOG_DEBUG(
      "[server] ReleaseFrame by publisher: %s", param.GetKey().c_str());
  senscord::server::PublisherAdapter* adapter =
      reinterpret_cast<senscord::server::PublisherAdapter*>(
          param.GetUserData());
  if (adapter) {
    senscord::Status status = adapter->ReleaseFrame(frameinfo);
    if (!status.ok()) {
      SENSCORD_SERVER_LOG_WARNING("%s", status.ToString().c_str());
    }
  }
}

}  // namespace

namespace senscord {
namespace server {

/**
 * @brief Open publisher adapter.
 * @return Status object.
 */
Status PublisherAdapter::Open(
    const MessageDataOpenPublisherRequest& msg, Core* core) {
  // open publisher
  Status status;
  if (!publisher_) {
    status = core->OpenPublisher(
        &publisher_, kServerNameLocalhost, msg.key, CallbackReleaseFrame);
    if (!status.ok()) {
      SENSCORD_SERVER_LOG_DEBUG("[server] failed to open: %s",
          status.ToString().c_str());
    } else {
      SENSCORD_SERVER_LOG_INFO("[server] open publisher: key=%s, id=%p",
          msg.key.c_str(), publisher_);
      publisher_->SetCallbackUserData(reinterpret_cast<uintptr_t>(this));
      resource_id_ = reinterpret_cast<uint64_t>(publisher_);
    }
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Close publisher adapter.
 * @return Status object.
 */
Status PublisherAdapter::Close(Core* core) {
  Status status;
  for (Allocators::iterator itr = allocators_.begin(), end = allocators_.end();
      itr != end; ++itr) {
    MemoryAllocator* allocator = itr->second;
    status = allocator->ExitMapping();
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  allocators_.clear();
  if (publisher_) {
    status = core->ClosePublisher(publisher_);
    publisher_ = NULL;
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Start to access to the stream.
 * @return Status object.
 */
Status PublisherAdapter::StartMonitoring() {
  if (thread_) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "already started");
  }
  if (publisher_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "invalid publisher pointer");
  }

  // create the monitor threads.
  end_flag_ = false;
  int32_t ret = osal::OSCreateThread(
      &thread_, PublisherAdapterThread, this, NULL);
  if (ret != 0) {
    StopMonitoring();
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseAborted,
        "failed to create monitor thread: %" PRIx32, ret);
  }
  return Status::OK();
}

/**
 * @brief Stop to access to the stream.
 * @return Status object.
 */
Status PublisherAdapter::StopMonitoring() {
  if (thread_ != NULL) {
    // stop threads.
    {
      osal::OSLockMutex(messaging_mutex_);
      end_flag_ = true;
      osal::OSBroadcastCond(messaging_cond_);
      osal::OSUnlockMutex(messaging_mutex_);
    }

    osal::OSJoinThread(thread_, NULL);

    // delete all remaining messages.
    {
      osal::OSLockMutex(messaging_mutex_);
      MessageList::iterator itr = messages_.begin();
      MessageList::iterator end = messages_.end();
      for (; itr != end; ++itr) {
        client_->ReleaseMessage(*itr);
      }
      messages_.clear();
      osal::OSUnlockMutex(messaging_mutex_);
    }
    thread_ = NULL;
  }
  return Status::OK();
}

/**
 * @brief The method to monitor new messages.
 */
void PublisherAdapter::Monitoring(MonitorType type) {
  SENSCORD_SERVER_LOG_DEBUG("[server](%p) start message monitoring: %d",
      publisher_, type);
  // detect message list
  MessageList* messages = &messages_;
  // monitoring loop
  while (!end_flag_) {
    const Message* msg = PopMessage(messages);
    if (msg == NULL) {
      // force stop
      break;
    }
    DoMessage(*msg);
    // not used anymore
    client_->ReleaseMessage(msg);
  }
  SENSCORD_SERVER_LOG_DEBUG("[server](%p) end message monitoring: %d",
      publisher_, type);
}

/**
 * @brief Push new message for this stream.
 * @param[in] (msg) New incoming message instance.
 */
void PublisherAdapter::PushMessage(const Message* msg) {
  if (msg) {
    osal::OSLockMutex(messaging_mutex_);
    if (!end_flag_) {
      messages_.push_back(msg);
      // notify to monitor threads
      osal::OSBroadcastCond(messaging_cond_);
    }
    osal::OSUnlockMutex(messaging_mutex_);
  }
}

/**
 * @brief Pop the latest messgae. if not incoming, wait forever.
 * @return The latest message.
 */
const Message* PublisherAdapter::PopMessage(MessageList* messages) {
  const Message* msg = NULL;

  osal::OSLockMutex(messaging_mutex_);
  // wait to arrive message.
  while (messages->empty() && (!end_flag_)) {
    if (osal::OSWaitCond(messaging_cond_, messaging_mutex_) < 0) {
      SENSCORD_SERVER_LOG_ERROR("[server](%p) failed to wait cond", publisher_);
      osal::OSUnlockMutex(messaging_mutex_);
      return NULL;
    }
  }

  // pop message
  if (!messages->empty() && (!end_flag_)) {
    msg = messages->front();
    messages->pop_front();
  }
  osal::OSUnlockMutex(messaging_mutex_);
  return msg;
}

/**
 * @brief Release multiple frames.
 * @param[in] (frames) List of frames to release.
 */
Status PublisherAdapter::ReleaseFrame(const FrameInfo& frameinfo) {
  Status status;
  bool is_memory_shared = false;
  for (std::vector<ChannelRawData>::const_iterator
      itr = frameinfo.channels.begin(), end = frameinfo.channels.end();
      itr != end; ++itr) {
    Memory* memory = itr->data_memory;
    MemoryAllocator* allocator = memory->GetAllocator();
    is_memory_shared |= allocator->IsMemoryShared();
    MemoryContained contained = {};
    contained.memory = itr->data_memory;
    contained.size = itr->data_size;
    contained.offset = itr->data_offset;
    Status status_unmap = allocator->Unmapping(contained);
    if (!status_unmap.ok()) {
      status = status_unmap;
      SENSCORD_SERVER_LOG_WARNING(
          "unmapping: id:%" PRIu32 "%s",
          itr->channel_id, status.ToString().c_str());
    }
  }
  if (status.ok() && is_memory_shared) {
    MessageDataReleaseFrameRequest msg_data = {};
    msg_data.sequence_number = frameinfo.sequence_number;
    status = client_->SendMessage(publisher_, frameinfo.sequence_number,
        kMessageTypeRequest, kMessageDataTypeReleaseFrame, msg_data);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Get the publisher pointer.
 * @return The pointer of this publisher.
 */
Publisher* PublisherAdapter::GetPublisher() const {
  return publisher_;
}

/**
 * @brief Do the calling function to stream each message types.
 * @param[in] (msg) The message for doing.
 */
void PublisherAdapter::DoMessage(const Message& msg) {
  Status status;

  if (msg.header.type == kMessageTypeSendFrame) {
    switch (msg.header.data_type) {
      case kMessageDataTypeSendFrame:
        status = PublishFrames(msg);
        break;
      default:
        // unknown data_type
        status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
            Status::kCauseInvalidOperation,
            "unknown request message: data_type=%d", msg.header.data_type);
        break;
    }
  } else if (msg.header.type == kMessageTypeReply) {
    switch (msg.header.data_type) {
      case kMessageDataTypeReleaseFrame:
        /* Do nothing */
        break;
      default:
        // unknown data_type
        status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
            Status::kCauseInvalidOperation,
            "unknown reply message: data_type=%d", msg.header.data_type);
        break;
    }
  } else {
    // unknown type
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation,
        "unknown message: type=%d, data_type=%d",
        msg.header.type, msg.header.data_type);
  }

  if (!status.ok()) {
    SENSCORD_SERVER_LOG_WARNING(status.ToString().c_str());
  }
}

/**
 * @brief Start stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status PublisherAdapter::PublishFrames(const Message& msg) {
  if (msg.data == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  }
  const MessageDataSendFrame& msg_data =
      *reinterpret_cast<const MessageDataSendFrame*>(msg.data);
  std::vector<uint64_t> reply_frames;
  std::vector<senscord::FrameInfo> frames;
  for (std::vector<senscord::MessageDataFrameLocalMemory>::const_iterator
      itr = msg_data.frames.begin(), end = msg_data.frames.end();
      itr != end; ++itr) {
    const senscord::MessageDataFrameLocalMemory& src_data = *itr;
    // update properties.
    Status tmp_status = UpdateFrameProperties(src_data);
    if (!tmp_status.ok()) {
      SENSCORD_STATUS_TRACE(tmp_status);
      SENSCORD_SERVER_LOG_WARNING(
          "[server] %s", tmp_status.ToString().c_str());
      // continue processing.
    }

    // check the need to reply
    bool reply = IsReplyToSendFrame(src_data);
    if (reply) {
      reply_frames.push_back(src_data.sequence_number);
    }

    frames.push_back(senscord::FrameInfo());
    senscord::FrameInfo& frameinfo = *frames.rbegin();

    // create frameinfo
    tmp_status = CreateFrameInfo(&frameinfo, src_data);
    if (!tmp_status.ok()) {
      SENSCORD_STATUS_TRACE(tmp_status);
      SENSCORD_SERVER_LOG_WARNING(
          "[server] %s", tmp_status.ToString().c_str());
      ReleaseFrame(frameinfo);
      continue;
    }
  }

  if (!frames.empty()) {
    // dropped frame does not send a reply as
    // the release frame message is sent.
    Status tmp_status = publisher_->PublishFrames(frames);
    if (!tmp_status.ok()) {
      SENSCORD_STATUS_TRACE(tmp_status);
      SENSCORD_SERVER_LOG_WARNING(
          "[server] %s", tmp_status.ToString().c_str());
      for (std::vector<senscord::FrameInfo>::const_iterator
          f_itr = frames.begin(), f_end = frames.end();
          f_itr != f_end; ++f_itr) {
        ReleaseFrame(*f_itr);
      }
    }
  }

  Status status;
  if (!reply_frames.empty()) {
    // create reply message.
    senscord::MessageDataSendFrameReply reply = {};
    reply.sequence_numbers.swap(reply_frames);
    status = client_->SendReply(msg, publisher_, reply);
    SENSCORD_STATUS_TRACE(status);
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Update properties by the frame from the server.
 * @param[in] (src) The frame message from the server.
 * @return Status object.
 */
Status PublisherAdapter::UpdateFrameProperties(
    const senscord::MessageDataFrameLocalMemory& src) {
  typedef std::vector<senscord::MessageDataChannelLocalMemory> SrcChannel;
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
      if (found != itr->updated_property_keys.end()) {
        // updating
        senscord::Status status = publisher_->UpdateChannelProperty(
            itr->channel_id, itr_prop->key, &itr_prop->property);
        if (!status.ok()) {
          return SENSCORD_STATUS_TRACE(status);
        }
      }
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Create the frame info for SendFrame.
 * @param[out] (dest) The destination of FrameInfo.
 * @param[in] (src) The frame message from the server.
 * @return Status object.
 */
senscord::Status PublisherAdapter::CreateFrameInfo(
    senscord::FrameInfo* dest,
    const senscord::MessageDataFrameLocalMemory& src) {
  if (dest == NULL) {
    return SENSCORD_STATUS_FAIL("client",
        senscord::Status::kCauseInvalidArgument, "dest is null");
  }

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
      MemoryContained contained = {};
      Status status = MemoryMapping(itr->allocator_key, rawdata, &contained);
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
bool PublisherAdapter::IsReplyToSendFrame(
    const MessageDataFrameLocalMemory& frame) const {
  for (std::vector<MessageDataChannelLocalMemory>::const_iterator
      itr = frame.channels.begin(), end = frame.channels.end();
      itr != end; ++itr) {
    if (itr->rawdata_info.delivering_mode != kDeliverAllData) {
      // If mode other than AllData is included, do not reply.
      return false;
    }
  }
  return true;
}

/**
 * @brief Memory mapping the rawdata.
 * @param[in] (key) The memory allocator key.
 * @param[in] (serialized) The serialized memory data.
 * @param[in] (memory) The mapped memory.
 * @return Status object.
 */
Status PublisherAdapter::MemoryMapping(
    const std::string& key, const std::vector<uint8_t>& serialized,
    MemoryContained* memory) {
  Status status;
  MemoryAllocator* allocator = NULL;
  Allocators::iterator itr = allocators_.find(key);
  if (itr == allocators_.end()) {
    MemoryManager* manager = MemoryManager::GetInstance();
    status = manager->GetAllocator(key, &allocator);
    if (status.ok()) {
      allocators_[key] = allocator;
      status = allocator->InitMapping();
      SENSCORD_STATUS_TRACE(status);
    }
  } else {
    allocator = itr->second;
  }
  if (status.ok()) {
    status = allocator->Mapping(serialized, memory);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Constructor.
 * @param[in] (client) The adapter to client.
 */
PublisherAdapter::PublisherAdapter(ClientAdapter* client)
    : publisher_(), client_(client), thread_(), end_flag_(false) {
  osal::OSCreateMutex(&messaging_mutex_);
  osal::OSCreateCond(&messaging_cond_);
}

/**
 * @brief Destructor.
 */
PublisherAdapter::~PublisherAdapter() {
  publisher_ = NULL;
  client_ = NULL;

  osal::OSDestroyMutex(messaging_mutex_);
  messaging_mutex_ = NULL;
  osal::OSDestroyCond(messaging_cond_);
  messaging_cond_ = NULL;
}

}  // namespace server
}   // namespace senscord
