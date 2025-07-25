/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stream_adapter.h"
#include <string>
#include <utility>    // make_pair
#include "senscord/osal_inttypes.h"
#include "senscord/osal.h"
#include "senscord/serialize.h"
#include "senscord/memory_allocator.h"
#include "server_log.h"
#include "client_adapter.h"

namespace senscord {
namespace server {

// total monitor thread number.
static const int kMonitorThreadNumber = 1;

/**
 * @brief The threading method for stream adapter.
 * @param[in] (arg) The pointer as stream adapter.
 * @return Don't care.
 */
static osal::OSThreadResult StreamAdapterThread(void* arg) {
  if (arg) {
    StreamAdapter* adapter = reinterpret_cast<StreamAdapter*>(arg);
    adapter->Monitoring(StreamAdapter::kMonitorStandard);
  }
  return static_cast<osal::OSThreadResult>(0);
}

/**
 * @brief The threading method for stream adapter only Lock/Unlock.
 * @param[in] (arg) The pointer as stream adapter.
 * @return Don't care.
 */
static osal::OSThreadResult StreamAdapterLockUnlockThread(void* arg) {
  if (arg) {
    StreamAdapter* adapter = reinterpret_cast<StreamAdapter*>(arg);
    adapter->Monitoring(StreamAdapter::kMonitorLockUnlock);
  }
  return static_cast<osal::OSThreadResult>(0);
}

/**
 * @brief The callback for new frame arrived.
 * @param[in] (stream) The stream pointer.
 * @param[in] (private_data) The pointer of stream adapter.
 */
static void FrameReceivedCallbackForServer(Stream* stream, void* private_data) {
  if (private_data) {
    StreamAdapter* adapter = reinterpret_cast<StreamAdapter*>(private_data);
    adapter->PublishingFrame();
  }
}

/**
 * @brief The callback for new event arrived.
 * @param[in] (stream) The stream where the event occurred.
 * @param[in] (event_type) The type of event.
 * @param[in] (args) Event argument.
 * @param[in] (private_data) The pointer of stream adapter.
 */
static void EventReceivedCallbackForServer(
    Stream* stream, const std::string& event_type,
    const EventArgument& args, void* private_data) {
  if (private_data) {
    StreamAdapter* adapter = reinterpret_cast<StreamAdapter*>(private_data);
    adapter->PublishingEvent(event_type, args);
  }
}

/**
  * @brief Close to the stream adapter.
  * @param[in] (core) Core object.
  * @return Status object.
  */
Status StreamAdapter::Close(Core* core) {
  return SENSCORD_STATUS_TRACE(core->CloseStream(stream_));
}

/**
 * @brief Start to access to the stream.
 * @return Status object.
 */
Status StreamAdapter::StartMonitoring() {
  if (threads_.size() > 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "already started");
  }
  if (stream_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "invalid stream pointer");
  }

  // register frame callback.
  Status status = stream_->RegisterFrameCallback(
      FrameReceivedCallbackForServer, this);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // create the monitor threads.
  end_flag_ = false;
  for (int count = 0; count < kMonitorThreadNumber; ++count) {
    osal::OSThread* thread = NULL;
    int32_t ret = osal::OSCreateThread(
        &thread, StreamAdapterThread, this, NULL);
    if (ret != 0) {
      StopMonitoring();
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          senscord::Status::kCauseAborted,
          "failed to create monitor thread: %" PRIx32, ret);
    }
    threads_.push_back(thread);
  }

  // create the property-locking thread.
  {
    int32_t ret = osal::OSCreateThread(&thread_lock_property_,
        StreamAdapterLockUnlockThread, this, NULL);
    if (ret != 0) {
      StopMonitoring();
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          senscord::Status::kCauseAborted,
          "failed to create the property-locking thread: %" PRIx32, ret);
    }
  }
  return Status::OK();
}

/**
 * @brief Stop to access to the stream.
 * @return Status object.
 */
Status StreamAdapter::StopMonitoring() {
  if (threads_.size() > 0) {
    // copy to tempolary
    std::vector<osal::OSThread*> tmp_threads = threads_;
    threads_.clear();

    // stop threads.
    {
      osal::OSLockMutex(messaging_mutex_);
      end_flag_ = true;
      osal::OSBroadcastCond(messaging_cond_);
      osal::OSUnlockMutex(messaging_mutex_);
    }

    // wait to stop all threads
    {
      ThreadList::iterator itr = tmp_threads.begin();
      ThreadList::const_iterator end = tmp_threads.end();
      for (; itr != end; ++itr) {
        osal::OSJoinThread((*itr), NULL);
      }
      tmp_threads.clear();
    }
    if (thread_lock_property_) {
      osal::OSJoinThread(thread_lock_property_, NULL);
      thread_lock_property_ = NULL;
    }

    // delete all remaining messages.
    {
      osal::OSLockMutex(messaging_mutex_);
      MessageList::iterator itr = messages_.begin();
      MessageList::iterator end = messages_.end();
      for (; itr != end; ++itr) {
        client_->ReleaseMessage(*itr);
      }
      messages_.clear();

      itr = messages_lock_property_.begin();
      end = messages_lock_property_.end();
      for (; itr != end; ++itr) {
        client_->ReleaseMessage(*itr);
      }
      messages_lock_property_.clear();
      osal::OSUnlockMutex(messaging_mutex_);
    }
  }
  return Status::OK();
}

/**
 * @brief The method to monitor new messages.
 */
void StreamAdapter::Monitoring(MonitorType type) {
  SENSCORD_SERVER_LOG_DEBUG("[server](%p) start message monitoring: %d",
      stream_, type);

  // detect message list
  MessageList* messages = &messages_;
  if (type == kMonitorLockUnlock) {
    messages = &messages_lock_property_;
  }

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
      stream_, type);
}

/**
 * @brief Push new message for this stream.
 * @param[in] (msg) New incoming message instance.
 */
void StreamAdapter::PushMessage(const Message* msg) {
  if (msg) {
    osal::OSLockMutex(messaging_mutex_);
    if (!end_flag_) {
      // push message
      if ((msg->header.data_type == kMessageDataTypeLockProperty) ||
          (msg->header.data_type == kMessageDataTypeUnlockProperty)) {
        // Lock/UnlockProperty request only
        messages_lock_property_.push_back(msg);
      } else {
        // other request
        messages_.push_back(msg);
      }

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
const Message* StreamAdapter::PopMessage(MessageList* messages) {
  const Message* msg = NULL;

  osal::OSLockMutex(messaging_mutex_);
  // wait to arrive message.
  while (messages->empty() && (!end_flag_)) {
    if (osal::OSWaitCond(messaging_cond_, messaging_mutex_) < 0) {
      SENSCORD_SERVER_LOG_ERROR("[server](%p) failed to wait cond", stream_);
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
 * @brief Publish the frame if arrived.
 */
void StreamAdapter::PublishingFrame() {
  // Get the number of frames that arrived.
  CurrentFrameNumProperty frame_num = {};
  stream_->GetProperty(kCurrentFrameNumPropertyKey, &frame_num);
  if (frame_num.arrived_number == 0) {
    // do nothing.
    return;
  }

  SENSCORD_SERVER_LOG_DEBUG(
      "[server](%p) arrived=%" PRId32 ", received=%" PRId32,
      stream_, frame_num.arrived_number, frame_num.received_number);

  // Get multiple frames from the stream.
  std::vector<Frame*> frames;
  frames.reserve(frame_num.arrived_number);
  GetFrames(frame_num.arrived_number, &frames);
  if (frames.empty()) {
    // do nothing.
    return;
  }

  // Send multiple frames to the client.
  Status status = SendFrames(frames);
  if (!status.ok()) {
    SENSCORD_SERVER_LOG_WARNING("[server](%p) failed to SendFrames: %s",
                                stream_, status.ToString().c_str());
  }
}

/**
 * @brief Get multiple frames from the stream.
 * @param[in] (max_number) The maximum number of frames to get.
 * @param[out] (frames) List of frames.
 */
void StreamAdapter::GetFrames(size_t max_number, std::vector<Frame*>* frames) {
  for (size_t i = 0; i < max_number; ++i) {
    Frame* frame = NULL;
    Status status = stream_->GetFrame(&frame, kTimeoutPolling);
    if ((!status.ok()) || (frame == NULL)) {
      SENSCORD_SERVER_LOG_DEBUG("[server](%p) failed to GetFrame", stream_);
      break;
    }
    frames->push_back(frame);
  }
}

/**
 * @brief Send the message about new frame to client.
 * @param[in] (frames) List of frames to send.
 * @return Status object.
 */
Status StreamAdapter::SendFrames(const std::vector<Frame*>& frames) {
  std::vector<ReleaseFrameInfo> pending_list;
  std::vector<ReleaseFrameInfo> release_list;

  // create message data payload
  MessageDataSendFrame msg_data = {};
  GetMessageDataForSendFrames(frames, &msg_data, &pending_list, &release_list);

  Status status;
  if (!msg_data.frames.empty()) {
    // Register the frame in the release pending list.
    for (std::vector<ReleaseFrameInfo>::const_iterator
        itr = pending_list.begin(), end = pending_list.end();
        itr != end; ++itr) {
      PushPendingReleaseFrame(*itr);
    }

    {
      uint64_t seq_num = msg_data.frames[0].sequence_number;
      status = client_->SendMessage(stream_, seq_num,
          kMessageTypeSendFrame, kMessageDataTypeSendFrame, msg_data);
      SENSCORD_STATUS_TRACE(status);
    }

    if (!status.ok()) {
      // Unregister pending frame.
      for (std::vector<ReleaseFrameInfo>::const_iterator
          itr = pending_list.begin(), end = pending_list.end();
          itr != end; ++itr) {
        uint64_t seq_num = 0;
        itr->frame->GetSequenceNumber(&seq_num);
        PopPendingReleaseFrame(seq_num);
      }
    }
  } else {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation,
        "There is no frame to send.");
  }

  if (!status.ok()) {
    // If it fails, it releases the pending list as well.
    release_list.insert(release_list.end(),
                        pending_list.begin(), pending_list.end());
  }

  // Release the frame registered in the release_list.
  ReleaseFrames(release_list);

  return status;
}

/**
 * @brief Release multiple frames.
 * @param[in] (frames) List of frames to release.
 */
void StreamAdapter::ReleaseFrames(
    const std::vector<ReleaseFrameInfo>& frames) {
  for (std::vector<ReleaseFrameInfo>::const_iterator
      itr = frames.begin(), end = frames.end(); itr != end; ++itr) {
    SENSCORD_SERVER_LOG_DEBUG("[server](%p) execute ReleaseFrameCore", stream_);
    Status status = ReleaseFrameCore(itr->frame, itr->rawdata_accessed);
    if (!status.ok()) {
      SENSCORD_SERVER_LOG_WARNING("[server](%p) failed to ReleaseFrame: %s",
                                  stream_, status.ToString().c_str());
    }
  }
}

/**
 * @brief Release the frame to the stream.
 * @param[in] (frame) The frame gotten.
 * @param[in] (is_rawdata_accessed) Whether the raw data has been accessed.
 * @return Status object of ReleaseFrame();
 */
Status StreamAdapter::ReleaseFrameCore(
    Frame* frame, bool is_rawdata_accessed) {
  if (stream_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "invalid stream");
  }
  Status status;
  if (is_rawdata_accessed) {
    status = stream_->ReleaseFrame(frame);
    SENSCORD_STATUS_TRACE(status);
  } else {
    status = stream_->ReleaseFrameUnused(frame);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Get the message data payload from the frame.
 * @param[out] (dest) The message data location.
 * @param[in] (src) The frame gotten from stream.
 * @param[out] (is_pending_release) Flag for pending release frame.
 * @param[out] (is_rawdata_accessed) Whether the raw data has been accessed.
 * @return Status object.
 */
Status StreamAdapter::GetMessageDataForSendFrame(
    MessageDataFrameLocalMemory* dest, const Frame* src,
    bool* is_pending_release, bool* is_rawdata_accessed) const {
  src->GetSequenceNumber(&dest->sequence_number);
  src->GetSentTime(&dest->sent_time);

  {
    Frame::UserData user_data = {};
    src->GetUserData(&user_data);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(user_data.address);
    dest->user_data.reserve(user_data.size);
    dest->user_data.assign(ptr, ptr + user_data.size);
  }

  // channel data
  ChannelList channels;
  src->GetChannelList(&channels);
  dest->channels.resize(channels.size());

  ChannelList::const_iterator itr = channels.begin();
  ChannelList::const_iterator end = channels.end();
  for (size_t ch_index = 0; itr != end; ++itr, ++ch_index) {
    Channel* ch = itr->second;
    MessageDataChannelLocalMemory& dest_ch = dest->channels[ch_index];

    // channel id
    dest_ch.channel_id = itr->first;

    // rawdata memory
    MemoryContained rawdata_memory = {};
    ch->GetRawDataMemory(&rawdata_memory);
    if (rawdata_memory.memory != NULL) {
      MemoryAllocator* allocator = rawdata_memory.memory->GetAllocator();
      dest_ch.allocator_key = allocator->GetKey();
    }

    // Get raw data from Connection class.
    Status status = client_->GetChannelRawData(ch, &dest_ch.rawdata_info);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }

    // rawdata type
    Channel::RawData rawdata = {};
    ch->GetRawData(&rawdata);
    dest_ch.rawdata_type = rawdata.type;

    // timestamp
    dest_ch.timestamp = rawdata.timestamp;

    // all properties
    std::vector<std::string> key_list;
    ch->GetPropertyList(&key_list);

    dest_ch.properties.resize(key_list.size());
    std::vector<std::string>::iterator key_itr = key_list.begin();
    std::vector<std::string>::iterator key_end = key_list.end();
    for (size_t prop_index = 0; key_itr != key_end; ++key_itr, ++prop_index) {
      MessageDataProperty& property = dest_ch.properties[prop_index];
      property.key = (*key_itr);
      status = ch->GetProperty(property.key, &property.property);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok()) {
#if 0   // for debug
        SENSCORD_SERVER_LOG_INFO("found updated property: %s, size=%" PRIdS,
            property.key.c_str(), property.property.data.size());
        for (size_t i = 0; i < property.property.data.size(); ++i) {
          osal::OSPrintf(" %02" PRIx8, property.property.data[i]);
        }
        osal::OSPrintf("\n");
#endif
      }
    }

    // updated properties
    dest_ch.updated_property_keys.clear();
    ch->GetUpdatedPropertyList(&dest_ch.updated_property_keys);

    if ((is_rawdata_accessed != NULL) &&
        (dest_ch.rawdata_info.delivering_mode == kDeliverAllData)) {
      *is_rawdata_accessed = true;
    }
  }

  if (is_pending_release != NULL) {
    *is_pending_release = true;
  }

  return Status::OK();
}

/**
 * Get the message data payload from multiple frames.
 * @param[in] (src) List of frames.
 * @param[out] (dest) The message data location.
 * @param[out] (pending_list) List containing pending frames.
 * @param[out] (release_list) List containing release frames.
 * @return Status object.
 */
void StreamAdapter::GetMessageDataForSendFrames(
    const std::vector<Frame*>& src, MessageDataSendFrame* dest,
    std::vector<ReleaseFrameInfo>* pending_list,
    std::vector<ReleaseFrameInfo>* release_list) const {
  dest->frames.resize(src.size());

  size_t count = 0;
  for (std::vector<Frame*>::const_iterator
      itr = src.begin(), end = src.end(); itr != end; ++itr) {
    bool pending = false;
    bool accessed = false;
    MessageDataFrameLocalMemory& msg_frame = dest->frames[count];

    Status status = GetMessageDataForSendFrame(
        &msg_frame, *itr, &pending, &accessed);

    ReleaseFrameInfo info = {};
    info.frame = *itr;
    info.rawdata_accessed = accessed;

    if (!status.ok()) {
      SENSCORD_SERVER_LOG_WARNING(
          "[server](%p) failed to GetMessageDataForSendFrame: %s",
          stream_, status.ToString().c_str());

      release_list->push_back(info);
      continue;
    }

    if (pending) {
      pending_list->push_back(info);
    } else {
      SENSCORD_SERVER_LOG_DEBUG("[server](%p) push_back release_list", stream_);
      release_list->push_back(info);
    }

    ++count;
  }

  if (dest->frames.size() != count) {
    SENSCORD_SERVER_LOG_DEBUG("[server](%p) resize the frames", stream_);
    dest->frames.resize(count);
  }
}

/**
 * @brief Publish the event.
 * @param[in] (event_type) The type of event.
 * @param[in] (args) Event argument.
 */
void StreamAdapter::PublishingEvent(
    const std::string& event_type, const EventArgument& args) {
  SENSCORD_SERVER_LOG_DEBUG("[server](%p) arrived new event: %s",
      stream_, event_type.c_str());

  // create the message
  MessageDataSendEvent msg_data = {};
  msg_data.event_type = event_type;
  msg_data.args = args;

  // send to client
  Status status = client_->SendMessage(stream_, event_send_count_++,
      kMessageTypeSendEvent, kMessageDataTypeSendEvent, msg_data);
  SENSCORD_STATUS_TRACE(status);
  if (!status.ok()) {
    SENSCORD_SERVER_LOG_WARNING("[server](%p) failed to SendEvent: %s",
        stream_, status.ToString().c_str());
  }
}

/**
 * @brief Do the calling function to stream each message types.
 * @param[in] (msg) The message for doing.
 */
void StreamAdapter::DoMessage(const Message& msg) {
  Status status;

  if (msg.header.type == kMessageTypeRequest) {
    switch (msg.header.data_type) {
      case kMessageDataTypeStart:
        status = Start(msg);
        break;

      case kMessageDataTypeStop:
        status = Stop(msg);
        break;

      case kMessageDataTypeGetProperty:
        status = GetProperty(msg);
        break;

      case kMessageDataTypeSetProperty:
        status = SetProperty(msg);
        break;

      case kMessageDataTypeLockProperty:
        status = LockProperty(msg);
        break;

      case kMessageDataTypeUnlockProperty:
        status = UnlockProperty(msg);
        break;

      case kMessageDataTypeReleaseFrame:
        status = ReleaseFrame(msg);
        break;

      case kMessageDataTypeRegisterEvent:
        status = RegisterEvent(msg);
        break;

      case kMessageDataTypeUnregisterEvent:
        status = UnregisterEvent(msg);
        break;

      case kMessageDataTypeGetPropertyList:
        status = GetPropertyList(msg);
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
       case kMessageDataTypeSendFrame:
         status = ReleaseFrameBySendFrameReply(msg);
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
Status StreamAdapter::Start(const Message& msg) {
  Status status = stream_->Start();
  SENSCORD_STATUS_TRACE(status);

  MessageDataStartReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Stop stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::Stop(const Message& msg) {
  Status status = stream_->Stop();
  SENSCORD_STATUS_TRACE(status);

  if (status.ok()) {
    // clear the arrived and unprocessed frames.
    stream_->ClearFrames(NULL);
  }

#if 0
  {
    // for debug
    CurrentFrameNumProperty prop = {};
    stream_->GetProperty(kCurrentFrameNumPropertyKey, &prop);
    SENSCORD_SERVER_LOG_DEBUG("remained frames: arriv=%d recv=%d",
        prop.arrived_number, prop.received_number);
  }
#endif

  MessageDataStopReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the property from stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::GetProperty(const Message& msg) {
  MessageDataGetPropertyReply reply_data = {};
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataGetPropertyRequest& msg_data =
        *reinterpret_cast<const MessageDataGetPropertyRequest*>(msg.data);
    reply_data.key = msg_data.key;
    reply_data.property = msg_data.property;
    status = stream_->GetProperty(reply_data.key, &reply_data.property);
    SENSCORD_STATUS_TRACE(status);
  }

  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the property from stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::SetProperty(const Message& msg) {
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataSetPropertyRequest& msg_data =
        *reinterpret_cast<const MessageDataSetPropertyRequest*>(msg.data);
    status = stream_->SetProperty(msg_data.key, &msg_data.property);
    SENSCORD_STATUS_TRACE(status);
  }

  MessageDataSetPropertyReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the property list from stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::GetPropertyList(const Message& msg) {
  MessageDataPropertyListReply reply_data = {};
  Status status = stream_->GetPropertyList(&reply_data.property_list);
  SENSCORD_STATUS_TRACE(status);

  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Lock the property from stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::LockProperty(const Message& msg) {
  MessageDataLockPropertyReply reply_data = {};
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataLockPropertyRequest& msg_data =
        *reinterpret_cast<const MessageDataLockPropertyRequest*>(msg.data);
    PropertyLockResource* resource_id = NULL;
    status = stream_->LockProperty(
        msg_data.keys, msg_data.timeout_msec, &resource_id);
    SENSCORD_STATUS_TRACE(status);
    reply_data.resource_id = reinterpret_cast<uint64_t>(resource_id);
  }

  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unlock the property from stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::UnlockProperty(const Message& msg) {
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataUnlockPropertyRequest& msg_data =
        *reinterpret_cast<const MessageDataUnlockPropertyRequest*>(msg.data);
    PropertyLockResource* lock_resource =
        reinterpret_cast<PropertyLockResource*>(msg_data.resource_id);
    status = stream_->UnlockProperty(lock_resource);
    SENSCORD_STATUS_TRACE(status);
  }

  MessageDataUnlockPropertyReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the frame to the stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::ReleaseFrame(const Message& msg) {
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataReleaseFrameRequest& msg_data =
        *reinterpret_cast<const MessageDataReleaseFrameRequest*>(msg.data);

    ReleaseFrameInfo pending_frame =
        PopPendingReleaseFrame(msg_data.sequence_number);
    if (pending_frame.frame != NULL) {
      bool rawdata_accessed = (pending_frame.rawdata_accessed ||
          msg_data.rawdata_accessed);

      status = ReleaseFrameCore(pending_frame.frame, rawdata_accessed);
      SENSCORD_STATUS_TRACE(status);
    }
  }

  MessageDataReleaseFrameReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release of frame by reply message.
 * @param[in] (msg) The message for doing.
 * @return Status object.
 */
Status StreamAdapter::ReleaseFrameBySendFrameReply(const Message& msg) {
  if (msg.data == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  }

  const MessageDataSendFrameReply& msg_data =
      *reinterpret_cast<const MessageDataSendFrameReply*>(msg.data);

  for (std::vector<uint64_t>::const_iterator
      itr = msg_data.sequence_numbers.begin(),
      end = msg_data.sequence_numbers.end(); itr != end; ++itr) {
    ReleaseFrameInfo pending_frame = PopPendingReleaseFrame(*itr);
    if (pending_frame.frame != NULL) {
      Status status = ReleaseFrameCore(pending_frame.frame, true);
      if (!status.ok()) {
        SENSCORD_SERVER_LOG_WARNING(
            "[server](%p) failed to release frame: seq_num=%" PRIu64
            ", status=%s", stream_, *itr, status.ToString().c_str());
      }
    }
  }

  return Status::OK();
}

/**
 * @brief Push the frame to release pending list.
 * @param[in] (pending_frame) Pending frame.
 */
void StreamAdapter::PushPendingReleaseFrame(
    const ReleaseFrameInfo& pending_frame) {
  if (pending_frame.frame != NULL) {
    uint64_t sequence_number = 0;
    pending_frame.frame->GetSequenceNumber(&sequence_number);

    osal::OSLockMutex(pending_frames_mutex_);
    SENSCORD_SERVER_LOG_DEBUG(
        "[server](%p) push pending release: seq_num=%" PRIx64,
        stream_, sequence_number);
    pending_release_frames_.insert(
        std::make_pair(sequence_number, pending_frame));
    osal::OSUnlockMutex(pending_frames_mutex_);
  }
}

/**
 * @brief Pop the frame from release pending list.
 * @param[in] (sequence_number) Sequence number of frame.
 * @return Pending frame.
 */
StreamAdapter::ReleaseFrameInfo StreamAdapter::PopPendingReleaseFrame(
    uint64_t sequence_number) {
  ReleaseFrameInfo pending_frame = {};
  osal::OSLockMutex(pending_frames_mutex_);
  PendingFrameMap::iterator itr =
      pending_release_frames_.find(sequence_number);
  SENSCORD_SERVER_LOG_DEBUG(
      "[server](%p) pop pending release: seq_num=%" PRIx64 "%s",
      stream_, sequence_number,
      (itr != pending_release_frames_.end()) ? "" : " (not found)");
  if (itr != pending_release_frames_.end()) {
    pending_frame = itr->second;
    pending_release_frames_.erase(itr);
  }
  osal::OSUnlockMutex(pending_frames_mutex_);
  return pending_frame;
}

/**
 * @brief Register event to the stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::RegisterEvent(const Message& msg) {
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataRegisterEventRequest& msg_data =
        *reinterpret_cast<const MessageDataRegisterEventRequest*>(msg.data);

    status = stream_->RegisterEventCallback(
        msg_data.event_type, EventReceivedCallbackForServer, this);
    SENSCORD_STATUS_TRACE(status);
  }

  MessageDataRegisterEventReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unregister event to the stream.
 * @param[in] (msg) The message for doing.
 * @return The status of send the reply.
 */
Status StreamAdapter::UnregisterEvent(const Message& msg) {
  Status status;
  if (msg.data == NULL) {
    status = SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg.data is null");
  } else {
    const MessageDataUnregisterEventRequest& msg_data =
        *reinterpret_cast<const MessageDataUnregisterEventRequest*>(msg.data);

    status = stream_->UnregisterEventCallback(msg_data.event_type);
    SENSCORD_STATUS_TRACE(status);
  }

  MessageDataUnregisterEventReply reply_data = {};
  reply_data.status.Set(status);

  // send reply
  status = client_->SendReply(msg, stream_, reply_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Constructor.
 * @param[in] (stream) The pointer of stream.
 * @param[in] (client) The adapter to client.
 */
StreamAdapter::StreamAdapter(Stream* stream, ClientAdapter* client)
    : stream_(stream), client_(client), thread_lock_property_(),
      end_flag_(false), event_send_count_() {
  resource_id_ = reinterpret_cast<uint64_t>(stream);
  osal::OSCreateMutex(&messaging_mutex_);
  osal::OSCreateCond(&messaging_cond_);
  osal::OSCreateMutex(&pending_frames_mutex_);
}

/**
 * @brief Destructor.
 */
StreamAdapter::~StreamAdapter() {
  stream_ = NULL;
  client_ = NULL;

  osal::OSDestroyMutex(messaging_mutex_);
  messaging_mutex_ = NULL;
  osal::OSDestroyCond(messaging_cond_);
  messaging_cond_ = NULL;
  osal::OSDestroyMutex(pending_frames_mutex_);
  pending_frames_mutex_ = NULL;
}

}  // namespace server
}   // namespace senscord
