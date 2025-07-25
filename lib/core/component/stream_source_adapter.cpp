/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "component/stream_source_adapter.h"
#include <algorithm>
#include <utility>
#include "component/stream_source_property_accessor.h"
#include "logger/logger.h"
#include "util/autolock.h"
#include "util/senscord_utils.h"
#include "allocator/memory_manager.h"

namespace senscord {

/**
 * @brief Constructor
 * @param[in] (core) Core instance.
 * @param[in] (port) Component port.
 * @param[in] (args) Component instance arguments.
 */
StreamSourceAdapter::StreamSourceAdapter(
    Core* core, ComponentPort* port, const ComponentArgument& args)
    : core_(core), port_(port), component_args_(args), port_args_(),
      source_(), thread_(), is_started_(false) {
  lock_manager_ = new StreamSourceFunctionLockManager();
}

/**
 * @brief Destructor
 */
StreamSourceAdapter::~StreamSourceAdapter() {
  RemovePropertyAll();
  source_ = NULL;
  delete lock_manager_;
  lock_manager_ = NULL;
}

/**
 * @brief Open the stream source.
 * @param[in] (args) Port argument.
 * @return Status object.
 */
Status StreamSourceAdapter::Open(const ComponentPortArgument& args) {
  if (core_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid core instance");
  }
  port_args_ = &args;

  StreamSourceFunctionLock lock(lock_manager_, kFunctionTypeState);
  Status status = source_->Open(core_, this);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Close the stream source.
 * @return Status object.
 */
Status StreamSourceAdapter::Close() {
  StreamSourceFunctionLock lock(lock_manager_, kFunctionTypeState);
  Status status = source_->Close();
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Start the stream source.
 * @return Status object.
 */
Status StreamSourceAdapter::Start() {
  StreamSourceFunctionLock lock(lock_manager_, kFunctionTypeState);
  Status status = source_->Start();
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Stop the stream source.
 * @return Status object.
 */
Status StreamSourceAdapter::Stop() {
  StreamSourceFunctionLock lock(lock_manager_, kFunctionTypeState);
  Status status = source_->Stop();
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the finished frame.
 * @param[in] (frameinfo) Finished frame infomation.
 * @param[in] (referenced_channel_ids) List of referenced channel IDs.
 *                                     (NULL is the same as empty)
 * @return Status object.
 */
Status StreamSourceAdapter::ReleaseFrame(
    const FrameInfo& frameinfo,
    const std::vector<uint32_t>* referenced_channel_ids) {
  StreamSourceFunctionLock lock(lock_manager_, kFunctionTypeReleaseFrame);
  Status status = source_->ReleaseFrame(frameinfo, referenced_channel_ids);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the unused frames.
 * @param[in] (frames) Finished and unused frames.
 */
void StreamSourceAdapter::ReleaseFramesUnused(
    const std::vector<const FrameInfo*>& frames) {
  std::vector<const FrameInfo*>::const_iterator itr = frames.begin();
  std::vector<const FrameInfo*>::const_iterator end = frames.end();
  for (; itr != end; ++itr) {
    if (*itr != NULL) {
      Status status = ReleaseFrame(*(*itr), NULL);
      SENSCORD_STATUS_TRACE(status);
      if (!status.ok()) {
        SENSCORD_LOG_WARNING(
            "failed to release frame (seq_num=%" PRIu64 "): %s",
            (*itr)->sequence_number, status.ToString().c_str());
      }
    }
  }
}

/**
 * @brief Start the publishing thread.
 * @return Status object.
 */
Status StreamSourceAdapter::StartThreading() {
  if (port_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to port creation");
  }

  is_started_ = true;
  int32_t ret = osal::OSCreateThread(&thread_, Threading, this, NULL);
  if (ret < 0) {
    is_started_ = false;
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation,
        "thread create error: 0x%" PRIx32, ret);
  }
  return Status::OK();
}

/**
 * @brief Notify to stop the publishing thread.
 */
void StreamSourceAdapter::StopThreadingNotify() {
  is_started_ = false;
}

/**
 * @brief Apply to stop the publishing thread.
 * @return Status object.
 */
Status StreamSourceAdapter::StopThreadingApply() {
  if (thread_ != NULL) {
    is_started_ = false;
    int32_t ret = osal::OSJoinThread(thread_, NULL);
    if (ret < 0) {
      SENSCORD_LOG_WARNING("failed to join thread: 0x%" PRIx32, ret);
    }
    thread_ = NULL;
  }
  return Status::OK();
}

/**
 * @brief Publishing the frames.
 */
void StreamSourceAdapter::Publishing() {
  SENSCORD_LOG_DEBUG("start publishing: %s", port_args_->stream_key.c_str());

  Status status;
  std::vector<FrameInfo> frames;
  std::vector<const FrameInfo*> drop_frames;

  while (is_started_) {
    // reset
    frames.clear();
    drop_frames.clear();

    // if state change(to stop), to finish
    if (lock_manager_->IsStateChanging()) {
      break;
    }
    // pull up frames from implemention
    source_->GetFrames(&frames);
    if (!is_started_) {
      // already stopped
      CollectAllFrames(frames, &drop_frames);
    } else {
      // send frames
      status = port_->SendFrames(frames, &drop_frames);
      if (!status.ok()) {
        // failed to send
        SENSCORD_STATUS_TRACE(status);
        source_->CatchErrorSendingFrame(status);
      }
    }

    // release dropped frames
    if (drop_frames.size() > 0) {
      ReleaseFramesUnused(drop_frames);
    }
  }
  SENSCORD_LOG_DEBUG("finish publishing: %s", port_args_->stream_key.c_str());
}

/**
 * @brief Thread for publishing frames.
 * @param[in] (arg) The porinter to this adapter.
 * @return always 0.
 */
osal::OSThreadResult StreamSourceAdapter::Threading(void* arg) {
  StreamSourceAdapter* adapter = reinterpret_cast<StreamSourceAdapter*>(arg);
  adapter->Publishing();
  return 0;
}

/**
 * @biref Reset the stream source informations.
 */
void StreamSourceAdapter::ResetSourceInformation() {
  RemovePropertyAll();
  source_ = NULL;
}

/**
 * @brief Get the argument value of instance by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetInstanceArgument(
    const std::string& name, std::string* value) const {
  Status status = util::GetArgument(component_args_.arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the argument value of instance by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetInstanceArgument(
    const std::string& name, int64_t* value) const {
  Status status = util::GetArgumentInt64(
      component_args_.arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the argument value of instance by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetInstanceArgument(
    const std::string& name, uint64_t* value) const {
  Status status = util::GetArgumentUint64(
      component_args_.arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the argument value of stream by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetStreamArgument(
    const std::string& name, std::string* value) const {
  Status status = util::GetArgument(port_args_->arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the argument value of stream by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetStreamArgument(
    const std::string& name, int64_t* value) const {
  Status status = util::GetArgumentInt64(port_args_->arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the argument value of stream by the name.
 * @param[in] (name) The argument name.
 * @param[out] (value) The argument value.
 * @return Status object.
 */
Status StreamSourceAdapter::GetStreamArgument(
    const std::string& name, uint64_t* value) const {
  Status status = util::GetArgumentUint64(port_args_->arguments, name, value);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the memory allocator by the name.
 * @param[in] (name) The memory allocator name.
 * @param[out] (allocator) The accessor of memory allocator.
 * @return Status object.
 */
Status StreamSourceAdapter::GetAllocator(
    const std::string& name, MemoryAllocator** allocator) const {
  if (allocator == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseInvalidArgument,
        "allocator is NULL");
  }
  std::map<std::string, MemoryAllocator*>::const_iterator itr =
      component_args_.allocators.find(name);
  if (itr != component_args_.allocators.end()) {
    *allocator = itr->second;
  } else {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotFound,
        "no allocator is existed by %s", name.c_str());
  }
  return Status::OK();
}

/**
 * @brief Send the event to the connected stream.
 * @param[in] (event_type) Event type to send.
 * @param[in] (args) Event argument.
 * @return Status object.
 */
Status StreamSourceAdapter::SendEvent(
    const std::string& event_type, const EventArgument& args) {
  Status status = port_->SendEvent(event_type, args);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Send the event for kEventError or kEventFatal.
 * @param[in] (error_status) Error status to send.
 * @return Status object.
 */
Status StreamSourceAdapter::SendEventError(const Status& error_status) {
  if (error_status.ok()) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseInvalidArgument,
        "Not an error status.");
  }
  Status status;
  EventArgument args;
  status = args.Set(kEventArgumentCause,
                    static_cast<int32_t>(error_status.cause()));
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  status = args.Set(kEventArgumentMessage, error_status.message());
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  if (error_status.level() == Status::kLevelFail) {
    status = SendEvent(kEventError, args);
    SENSCORD_STATUS_TRACE(status);
  } else if (error_status.level() == Status::kLevelFatal) {
    status = SendEvent(kEventFatal, args);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Send the event for kEventFrameDropped.
 * @param[in] (sequence_number) The sequence number that was dropped.
 * @return Status object.
 */
Status StreamSourceAdapter::SendEventFrameDropped(uint64_t sequence_number) {
  Status status;
  EventArgument args;
  status = args.Set(kEventArgumentSequenceNumber, sequence_number);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  status = SendEvent(kEventFrameDropped, args);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Send the event for kEventPropertyUpdated.
 * @param[in] (property_key) The key of the updated property.
 * @return Status object.
 */
Status StreamSourceAdapter::SendEventPropertyUpdated(
    const std::string& property_key) {
  Status status;
  // check property key.
  {
    util::AutoLock autolock(&mutex_properties_);
    if (properties_.find(
        senscord::PropertyUtils::GetKey(property_key)) == properties_.end()) {
      return SENSCORD_STATUS_FAIL(
          kStatusBlockCore, Status::kCauseNotFound,
          "The specified Property key not found.");
    }
  }
  EventArgument args;
  status = args.Set(kEventArgumentPropertyKey,
      senscord::PropertyUtils::GetKey(property_key));
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  status = SendEvent(kEventPropertyUpdated, args);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Register property accessor.
 * @param[in] (accessor) Property accessor.
 * @return Status object.
 */
Status StreamSourceAdapter::RegisterPropertyAccessor(
    PropertyAccessor* accessor) {
  // wrap for function lock
  StreamSourcePropertyAccessor* source_accessor =
      new StreamSourcePropertyAccessor(accessor->GetKey(), lock_manager_);
  Status status = port_->RegisterPropertyAccessor(source_accessor);
  if (status.ok()) {
    util::AutoLock autolock(&mutex_properties_);
    source_accessor->SetPropertyAccessor(accessor);
    properties_.insert(accessor->GetKey());
  } else {
    delete source_accessor;
  }
  return SENSCORD_STATUS_TRACE(status);
}

#ifdef SENSCORD_SERIALIZE
/**
 * @brief Update the serialized property for frame channel.
 * @param[in] (channel_id) Target channel ID.
 * @param[in] (key) Property key to updated.
 * @param[in] (property) Serialized property.
 * @param[in] (property_size) Serialized property size.
 * @return Status object.
 */
Status StreamSourceAdapter::UpdateFrameSerializedProperty(uint32_t channel_id,
    const std::string& key, const void* property, size_t property_size) {
  Status status = port_->UpdateFrameSerializedProperty(
      channel_id, senscord::PropertyUtils::GetKey(key),
      property, property_size);
  return SENSCORD_STATUS_TRACE(status);
}
#else
/**
 * @brief Update frame channel property.
 * @param[in] (channel_id) Target channel ID.
 * @param[in] (key) Property key to updated.
 * @param[in] (property) Property to updated.
 * @param[in] (factory) Property factory.
 * @return Status object.
 */
Status StreamSourceAdapter::UpdateFramePropertyWithFactory(
    uint32_t channel_id, const std::string& key, const void* property,
    const PropertyFactoryBase& factory) {
  Status status = port_->UpdateFramePropertyWithFactory(
      channel_id, senscord::PropertyUtils::GetKey(key), property, factory);
  return SENSCORD_STATUS_TRACE(status);
}
#endif  // SENSCORD_SERIALIZE

/**
 * @brief Collect the all frames.
 * @param[in] (in) The base list of frames.
 * @param[out] (out) The collected list of frames.
 */
void StreamSourceAdapter::CollectAllFrames(
    const std::vector<FrameInfo>& in,
    std::vector<const FrameInfo*>* out) const {
  if (in.size() > 0) {
    std::vector<FrameInfo>::const_iterator itr = in.begin();
    std::vector<FrameInfo>::const_iterator end = in.end();
    for (; itr != end; ++itr) {
      out->push_back(&(*itr));
    }
  }
}

/**
 * @brief Remove all property accessors.
 */
void StreamSourceAdapter::RemovePropertyAll() {
  util::AutoLock autolock(&mutex_properties_);
  while (!properties_.empty()) {
    std::set<std::string>::iterator itr = properties_.begin();
    PropertyAccessor* accessor = NULL;
    port_->UnregisterPropertyAccessor(*itr, &accessor);
    delete accessor;
    properties_.erase(itr);
  }
}

}  // namespace senscord
