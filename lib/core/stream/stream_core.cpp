/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stream/stream_core.h"

#include <stdint.h>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <utility>      // std::make_pair
#include <algorithm>    // std::sort

#include "logger/logger.h"
#include "senscord/develop/property_types_private.h"
#include "senscord/develop/deserialized_property_accessor.h"
#include "frame/frame_manager.h"
#include "frame/frame_manager_factory.h"
#include "component/component_manager.h"
#include "component/component_adapter.h"
#include "util/mutex.h"
#include "util/autolock.h"
#include "stream/frame_observer_order.h"
#include "stream/event_observer_order.h"
#include "stream/stream_function_lock_manager.h"
#include "stream/stream_shared_property_accessor.h"
#include "extension/extension_manager.h"

#include "allocator/memory_manager.h"
#include "frame/frame_core.h"

namespace {

const int32_t kUserDataPropertyMaxSize = 256;

/**
 * @brief Registers an internal property with the stream.
 * @param[in] (property_key) The key of property.
 * @param[in] (stream) The target stream.
 */
template <typename T>
void RegisterInternalProperty(
    const std::string& property_key, senscord::StreamCore* stream) {
#ifdef SENSCORD_SERIALIZE
  senscord::PropertyAccessor* accessor =
      new senscord::DeserializedPropertyAccessor<senscord::StreamCore, T>(
          property_key, stream);
#else
  senscord::PropertyAccessor* accessor =
      new senscord::FastPropertyAccessor<senscord::StreamCore, T>(
          property_key, stream);
#endif  // SENSCORD_SERIALIZE
  stream->RegisterInternalPropertyAccessor(accessor);
}

/**
 * @brief Registers a shared property with the stream.
 * @param[in] (property_key) The key of property.
 * @param[in] (stream) The target stream.
 */
template <typename T>
void RegisterSharedProperty(
    const std::string& property_key, senscord::StreamCore* stream) {
  senscord::PropertyAccessor* accessor =
      new senscord::StreamSharedPropertyAccessor<senscord::StreamCore, T>(
          property_key, stream);
  stream->RegisterSharedPropertyAccessor(accessor);
}

}  // namespace

namespace senscord {

/**
 * @brief Constructor
 */
StreamCore::StreamCore()
    : frame_manager_(NULL),
      recorder_(this),
      adapter_(NULL),
      frame_observer_(NULL),
      history_book_(NULL),
      frame_extension_(NULL),
      lock_resource_(NULL) {
  lock_manager_ = new StreamFunctionLockManager();
  osal::OSCreateCond(&cond_frame_);

  // register standard properties.
  RegisterInternalProperty<StreamTypeProperty>(
      kStreamTypePropertyKey, this);
  RegisterInternalProperty<StreamKeyProperty>(
      kStreamKeyPropertyKey, this);
  RegisterInternalProperty<StreamStateProperty>(
      kStreamStatePropertyKey, this);
  RegisterInternalProperty<FrameBuffering>(
      kFrameBufferingPropertyKey, this);
  RegisterInternalProperty<CurrentFrameNumProperty>(
      kCurrentFrameNumPropertyKey, this);
#ifdef SENSCORD_RECORDER
  RegisterInternalProperty<RecordProperty>(
      kRecordPropertyKey, this);
  RegisterInternalProperty<RecorderListProperty>(
      kRecorderListPropertyKey, this);
#endif  // SENSCORD_RECORDER

  RegisterSharedProperty<UserDataProperty>(
      kUserDataPropertyKey, this);
  RegisterSharedProperty<ChannelMaskProperty>(
      kChannelMaskPropertyKey, this);
  RegisterSharedProperty<SkipFrameProperty>(
      kSkipFramePropertyKey, this);
#ifdef SENSCORD_SERVER
  RegisterSharedProperty<FrameExtensionProperty>(
      kFrameExtensionPropertyKey, this);
#endif
}

/**
 * @brief Destructor
 */
StreamCore::~StreamCore() {
  CloseStreamExtension(false);
  {
    util::AutoLock lock(&mutex_frame_);
    if (frame_manager_ != NULL) {
      FrameManagerFactory::DestroyInstance(frame_manager_);
      frame_manager_ = NULL;
    }
  }
  {
    util::AutoLock lock(&mutex_property_);
    while (!property_list_.empty()) {
      PropertyAccessorList::iterator itr = property_list_.begin();
      delete itr->second;
      property_list_.erase(itr);
    }
    while (!property_list_shared_.empty()) {
      PropertyAccessorList::iterator itr = property_list_shared_.begin();
      delete itr->second;
      property_list_shared_.erase(itr);
    }
  }
  {
    util::AutoLock lock(&mutex_callback_frame_);
    delete frame_observer_;
    frame_observer_ = NULL;
  }
  {
    util::AutoLock lock(&mutex_callback_event_);
    while (!event_observer_.empty()) {
      std::map<std::string, EventObserver*>::iterator itr =
          event_observer_.begin();
      delete itr->second;
      event_observer_.erase(itr);
    }
  }
  osal::OSDestroyCond(cond_frame_);
  delete lock_manager_;
  lock_manager_ = NULL;
  delete frame_extension_;
  frame_extension_ = NULL;
}

/**
 * @brief Initialize and setup this stream.
 * @param (config) Stream configuration.
 * @return Status object.
 */
Status StreamCore::Init(const StreamSetting& config) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateInit);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  config_ = config;

  SENSCORD_LOG_DEBUG("[[[ stream : %p ]]]", this);
  SENSCORD_LOG_DEBUG("- stream key: %s", config_.stream_key.c_str());
  SENSCORD_LOG_DEBUG("- instance name: %s",
      config_.address.instance_name.c_str());
  SENSCORD_LOG_DEBUG("  port type: %s", config_.address.port_type.c_str());
  SENSCORD_LOG_DEBUG("  port id: %" PRId32, config_.address.port_id);
  SENSCORD_LOG_DEBUG("- radical instance name: %s",
      config_.radical_address.instance_name.c_str());
  SENSCORD_LOG_DEBUG("  radical port type: %s",
      config_.radical_address.port_type.c_str());
  SENSCORD_LOG_DEBUG("  radical port id: %" PRId32,
      config_.radical_address.port_id);

  CommitLocalStateChange();
  return Status::OK();
}

/**
 * @brief Finalize this stream.
 * @return Status object.
 */
Status StreamCore::Exit() {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateNotInit);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  config_.address.port_type.clear();
  config_.address.port_id = -1;
  CommitLocalStateChange();
  resources_.ReleaseAll();
  return Status::OK();
}

/**
 * @brief Open this stream.
 * @param[in] (core_behavior) Core behavior.
 * @return Status object.
 */
Status StreamCore::Open(const CoreBehavior* core_behavior) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateReady);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  {
    status = ComponentManager::GetInstance()->OpenComponent(
      config_.address.instance_name, core_behavior, &adapter_);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      if (adapter_ == NULL) {
        status = SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseUnknown,
            "adapter is null");
      }
      if (status.ok()) {
        // setup frame manager
        status = CreateFrameManager(config_.frame_buffering);
        SENSCORD_STATUS_TRACE(status);
      }
      // open
      if (status.ok()) {
        status = adapter_->Open(config_.address.port_type,
            config_.address.port_id, this);
        SENSCORD_STATUS_TRACE(status);

        // setup property
        history_book_ = adapter_->GetPropertyHistoryBook(
            config_.address.port_type, config_.address.port_id);
      }
      // failed
      if (!status.ok()) {
        DestroyFrameManager();
        history_book_ = NULL;
        ComponentManager::GetInstance()->CloseComponent(adapter_);
      }
    }
  }

  if (status.ok()) {
    CommitLocalStateChange();

    // Execute `StreamExtension::Open`
    status = OpenStreamExtension();
    if (!status.ok()) {
      Close();
    }
  } else {
    CancelLocalStateChange();
  }
  return status;
}

/**
 * @brief Close this stream.
 * @return Status object.
 */
Status StreamCore::Close() {
  bool stop_done = false;
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateReady);
  if (status.ok()) {
    status = StopCore();
    if (!status.ok()) {
      CancelLocalStateChange();
      return SENSCORD_STATUS_TRACE(status);
    }
    stop_done = true;
  } else {
    StreamLocalState state = GetLocalState();
    if (state != kStreamLocalStateReady) {
      return SENSCORD_STATUS_TRACE(status);
    }
  }

  // Execute `StreamExtension::Close`
  status = CloseStreamExtension(true);
  if (!status.ok()) {
    CancelLocalStateChange();
    return SENSCORD_STATUS_TRACE(status);
  }

  status = BeginLocalStateChange(kStreamLocalStateInit);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  lock_manager_->WaitAccessDone(kStreamFunctionTypeComponent);

  do {
    {
      util::AutoLock lock(&mutex_frame_);
      if (frame_manager_ != NULL) {
        status = frame_manager_->Exit();
        if (!status.ok()) {
          SENSCORD_STATUS_TRACE(status);
          break;
        }
      }
    }
    UnregisterFrameCallback();
    UnregisterEventCallbackAll();
    status = adapter_->Close(config_.address.port_type,
        config_.address.port_id, this);
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      if (frame_manager_ != NULL) {
        frame_manager_->Init(config_.frame_buffering.num, this);
      }
      break;
    }
    status = DestroyFrameManager();
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      break;
    }
    history_book_ = NULL;
    lock_manager_->WaitAccessDone(kStreamFunctionTypeInternal);
    status = ComponentManager::GetInstance()->CloseComponent(adapter_);
    SENSCORD_STATUS_TRACE(status);
  } while (false);

  if (status.ok()) {
    adapter_ = NULL;
    CommitLocalStateChange();
    if (stop_done) {
      SENSCORD_LOG_WARNING("closed the running stream. stream_key=%s",
                           config_.stream_key.c_str());
    }
    // transition to the NotInit state in this function lock section.
    Exit();
  } else {
    CancelLocalStateChange();
  }
  return status;
}

/**
 * @brief Start this stream.
 * @return Status object.
 */
Status StreamCore::Start() {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateRunning);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  {
    StartFrameObserver();
    status = adapter_->Start(config_.address.port_type,
        config_.address.port_id, this);
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      StopFrameObserver();
    }
  }

  if (status.ok()) {
    CommitLocalStateChange();
  } else {
    CancelLocalStateChange();
  }
  return status;
}

/**
 * @brief Stop this stream.
 * @return Status object.
 */
Status StreamCore::Stop() {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeState);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = BeginLocalStateChange(kStreamLocalStateReady);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  status = StopCore();
  if (status.ok()) {
    CommitLocalStateChange();
  } else {
    CancelLocalStateChange();
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Stop main process.
 * @return Status object.
 */
Status StreamCore::StopCore() {
  Status status = adapter_->Stop(config_.address.port_type,
      config_.address.port_id, this);
  if (status.ok()) {
    // Below is the process required for GetFrame() in FrameCallback.
    // Change only the current state and cancel the GetFrame() function.
    // Exclusivity with other threads will continue until it is committed.
    {
      util::AutoLock frame_lock(&mutex_frame_);
      CommitLocalStateChange();
      WakeupWaitFrame(&mutex_frame_);
    }
    StopFrameObserver();

    // force stop recording
    recorder_.Stop();
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the received frame.
 * @param[out] (frame) Location of received frame.
 * @param[in] (timeout_msec) Time of wait msec if no received.
 *                           0 is polling, minus is forever.
 * @return Status object.
 */
Status StreamCore::GetFrame(Frame** frame, const int32_t timeout_msec) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (frame == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "frame is null");
  }

  // Setup timeout time.
  uint64_t abstime = 0;
  if (timeout_msec > 0) {
    osal::OSGetTime(&abstime);
    abstime += static_cast<uint64_t>(timeout_msec) * 1000 * 1000;
  }

  while (true) {
    util::AutoLock lock(&mutex_frame_);
    if (frame_manager_ == NULL) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation, "stream was closing.");
    }

    StreamLocalState state = GetLocalState();
    if (state != kStreamLocalStateRunning) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation, "invalid state.");
    }

    Status status = frame_manager_->Get(frame);
    if (status.ok()) {
      break;
    }

    if (timeout_msec == kTimeoutPolling) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseTimeout,
          "no frame received.");
    } else if (timeout_msec > 0) {
      // timeout wait
      int32_t ret = osal::OSTimedWaitCond(cond_frame_, mutex_frame_.GetObject(),
          abstime);
      if (ret < 0) {
        return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseTimeout,
            "no frame received.");
      }
    } else {
      // forever wait
      osal::OSWaitCond(cond_frame_, mutex_frame_.GetObject());
    }
  }

  // frame extension
  {
    if (frame_extension_) {
      ExtensionFrameInfo frame_info = {};
      FrameCore* frame_core = static_cast<FrameCore*>(*frame);
      frame_core->GetSequenceNumber(&frame_info.sequence_number);
      frame_core->SetDisableChannelMask(true);
      frame_extension_->ExtendFrame(frame_core, &frame_info);
      frame_core->SetDisableChannelMask(false);
      PropertyHistoryBook* history_book =
          frame_extension_->GetPropertyHistoryBook();
      frame_core->SetExtensionFrameInfo(&frame_info, history_book);
    }
  }

  // frame arrived, and recording.
  recorder_.PushFrame(*frame);

  return Status::OK();
}

/**
 * @brief Release the gotten frame.
 * @param[in] (frame) Received frame by GetFrame().
 * @return Status object.
 */
Status StreamCore::ReleaseFrame(Frame* frame) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeComponent);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = ReleaseFrameCore(frame, true);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the gotten frame.
 *
 * Use this function if you do not refer to the raw data of the channel.
 *
 * @param[in] (frame) Received frame by GetFrame().
 * @return Status object.
 */
Status StreamCore::ReleaseFrameUnused(Frame* frame) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeComponent);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  Status status = ReleaseFrameCore(frame, false);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the gotten frame.
 * @param[in] (frame) Received frame by GetFrame().
 * @param[in] (rawdata_accessed) Whether you have accessed raw data.
 * @return Status object.
 */
Status StreamCore::ReleaseFrameCore(Frame* frame, bool rawdata_accessed) {
  if (frame == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "frame is null");
  }
  {
    StreamLocalState state = GetLocalState();
    if ((state != kStreamLocalStateRunning) &&
        (state != kStreamLocalStateReady)) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
    }
  }

  // frame extension
  {
    if (frame_extension_) {
      FrameCore* frame_core = static_cast<FrameCore*>(frame);
      ExtensionFrameInfo* frame_info = frame_core->GetExtensionFrameInfo();
      if (frame_info != NULL) {
        frame_extension_->ReleaseFrame(*frame_info);
        frame_core->SetExtensionFrameInfo(NULL, NULL);
      }
      rawdata_accessed = true;  // force setting
    }
  }

  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return Status::OK();  // already destroyed
  }
  Status status = frame_manager_->Remove(frame, rawdata_accessed);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Clear frames have not gotten.
 * @param[out] (frame_number) number of cleared frames.
 * @return Status object.
 */
Status StreamCore::ClearFrames(int32_t* frame_number) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeComponent);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  {
    StreamLocalState state = GetLocalState();
    if ((state != kStreamLocalStateRunning) &&
        (state != kStreamLocalStateReady)) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
    }
  }

  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return Status::OK();  // already destroyed
  }
  Status status = frame_manager_->Clear(frame_number);
  return SENSCORD_STATUS_TRACE(status);
}

#ifdef SENSCORD_SERIALIZE
/**
 * @brief Get the serialized property.
 * @param[in] (property_key) Key of property to get.
 * @param[in] (input_property) Location of input property.
 * @param[in] (input_property_size) Size of input property.
 * @param[out] (output_property) Location of output property.
 * @param[out] (output_property_size) Size of output property.
 * @return Status object.
 */
Status StreamCore::GetSerializedProperty(
    const std::string& property_key,
    const void* input_property,
    size_t input_property_size,
    void** output_property,
    size_t* output_property_size) {
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  {
    PropertyAccessor* property_entry =
        GetInternalPropertyAccessor(property_key);
    if (property_entry != NULL) {
      // Internal property.
      Status status = property_entry->Get(property_key, input_property,
          input_property_size, output_property, output_property_size);
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  // Other property.
  Status status = adapter_->GetSerializedProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, input_property, input_property_size, output_property,
      output_property_size);

  // frame extention merge channel info
  if (property_key == kChannelInfoPropertyKey &&
      frame_extension_ && status.ok()) {
    // deserialize
    ChannelInfoProperty property = {};
    serialize::Decoder decoder(*output_property, *output_property_size);
    status = decoder.Pop(property);
    SENSCORD_STATUS_TRACE(status);
    if (status.ok()) {
      // merge
      std::map<uint32_t, ChannelInfo> channels =
          frame_extension_->GetChannelInfo();
      property.channels.insert(channels.begin(), channels.end());
      // serialize
      serialize::SerializedBuffer buffer;
      serialize::Encoder encoder(&buffer);
      status = encoder.Push(property);
      SENSCORD_STATUS_TRACE(status);
      if (status.ok()) {
        delete[] reinterpret_cast<uint8_t*>(*output_property);
        const size_t buffer_size = buffer.size();
        uint8_t* new_buffer = new uint8_t[buffer_size];
        serialize::Memcpy(new_buffer, buffer_size, buffer.data(), buffer_size);
        *output_property = new_buffer;
        *output_property_size = buffer_size;
      }
    }
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Release the serialized property.
 * @param[in] (property_key) Key of property to release.
 * @param[in] (property) Location of property.
 * @param[in] (property_size) Size of property.
 * @return Status object.
 */
Status StreamCore::ReleaseSerializedProperty(
    const std::string& property_key,
    void* property,
    size_t property_size) {
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  {
    PropertyAccessor* property_entry =
        GetInternalPropertyAccessor(property_key);
    if (property_entry == NULL) {
      property_entry = GetSharedPropertyAccessor(property_key);
    }
    if (property_entry != NULL) {
      // Internal or Shared property.
      Status status = property_entry->Release(property_key, property,
          property_size);
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  // Other property.
  Status status = adapter_->ReleaseSerializedProperty(
      config_.address.port_type, config_.address.port_id,
      property_key, property, property_size);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the serialized property.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) Location of property.
 * @param[in] (property_size) Size of property.
 * @return Status object.
 */
Status StreamCore::SetSerializedProperty(
    const std::string& property_key,
    const void* property,
    size_t property_size) {
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  {
    PropertyAccessor* property_entry =
        GetInternalPropertyAccessor(property_key);
    if (property_entry != NULL) {
      // Internal property.
      Status status = property_entry->Set(property_key, property,
          property_size);
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  // Other property.
  Status status = adapter_->SetSerializedProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, property, property_size);
  return SENSCORD_STATUS_TRACE(status);
}
#else
/**
 * @brief Get the property.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Location of property.
 * @return Status object.
 */
Status StreamCore::GetProperty(
    const std::string& property_key, void* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  StreamFunctionLock lock(lock_manager_, kStreamFunctionTypeComponent);
  if (!lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(lock.GetStatus());
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  {
    PropertyAccessor* property_entry =
        GetInternalPropertyAccessor(property_key);
    if (property_entry != NULL) {
      // Internal property.
      Status status = property_entry->Get(property_key, property);
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  // Other property.
  Status status = adapter_->GetProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, property);

  // frame extention merge channel info
  {
    if (property_key == kChannelInfoPropertyKey &&
        frame_extension_ && status.ok()) {
      ChannelInfoProperty* src_property =
          static_cast<ChannelInfoProperty*>(property);
      std::map<uint32_t, ChannelInfo> channels =
          frame_extension_->GetChannelInfo();
      src_property->channels.insert(channels.begin(), channels.end());
    }
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the property with key.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) Location of property.
 * @return Status object.
 */
Status StreamCore::SetProperty(
    const std::string& property_key, const void* property) {
  StreamFunctionLock lock(lock_manager_, kStreamFunctionTypeComponent);
  if (!lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(lock.GetStatus());
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  {
    PropertyAccessor* property_entry =
        GetInternalPropertyAccessor(property_key);
    if (property_entry != NULL) {
      // Internal property.
      Status status = property_entry->Set(property_key, property);
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  // Other property.
  Status status = adapter_->SetProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, property);
  return SENSCORD_STATUS_TRACE(status);
}
#endif  // SENSCORD_SERIALIZE

/**
 * @brief Get the supported property key list on this stream.
 * @param[out] (key_list) Supported property key list.
 * @return Status object.
 */
Status StreamCore::GetPropertyList(
    std::vector<std::string>* key_list) const {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (key_list == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "key_list is null");
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }

  // get the list of component supported.
  std::set<std::string> list;
  Status status = adapter_->GetSupportedPropertyList(
      config_.address.port_type, config_.address.port_id, &list);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // get the list of this stream supported.
  {
    util::AutoLock lock(&mutex_property_);
    for (PropertyAccessorList::const_iterator
        itr = property_list_.begin(), end = property_list_.end();
        itr != end; ++itr) {
      list.insert(itr->first);
    }
    for (PropertyAccessorList::const_iterator
        itr = property_list_shared_.begin(), end = property_list_shared_.end();
        itr != end; ++itr) {
      list.insert(itr->first);
    }
  }

  key_list->assign(list.begin(), list.end());
  return Status::OK();
}

/**
 * @brief Lock to access properties.
 * @param[in] (timeout_msec) Time of wait msec if locked already.
 *                           0 is polling, minus is forever.
 * @return Status object.
 */
Status StreamCore::LockProperty(int32_t timeout_msec) {
  std::set<std::string> keys;
  PropertyLockResource* lock_resource = NULL;
  Status status = LockProperty(keys, timeout_msec, &lock_resource);
  if (status.ok()) {
    lock_resource_ = lock_resource;
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
  * @brief Lock to access properties.
  * @param[in] (keys) Property keys for lock targets.
  * @param[in] (timeout_msec) Time of wait msec if locked already.
  *                           0 is polling, minus is forever.
  * @param[out] (lock_resource) Locked properties resource.
  * @return Status object.
  */
Status StreamCore::LockProperty(
    const std::set<std::string>& keys,
    int32_t timeout_msec,
    PropertyLockResource** lock_resource) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (lock_resource == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "lock_resource is NULL");
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }

  std::set<util::PropertyKey> lock_targets;
  if (keys.empty()) {
    std::set<std::string> key_list;
    // key list of properties excluding internal property
    Status status = adapter_->GetSupportedPropertyList(
        config_.address.port_type, config_.address.port_id, &key_list);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
    {
      util::AutoLock lock(&mutex_property_);
      for (PropertyAccessorList::const_iterator
          itr = property_list_shared_.begin(),
          end = property_list_shared_.end(); itr != end; ++itr) {
        key_list.insert(itr->first);
      }
    }
    // remove private keys
    key_list.erase(kRegisterEventPropertyKey);
    key_list.erase(kUnregisterEventPropertyKey);
    key_list.erase(kFrameExtensionPropertyKey);
    // convert parse keys
    for (std::set<std::string>::const_iterator itr = key_list.begin(),
        end = key_list.end(); itr != end; ++itr) {
      util::PropertyKey parser(*itr);
      lock_targets.insert(parser);
    }
  } else {
    // convert parse keys
    for (std::set<std::string>::const_iterator itr = keys.begin(),
        end = keys.end(); itr != end; ++itr) {
      util::PropertyKey parser(*itr);
      lock_targets.insert(parser);
    }
    for (std::set<util::PropertyKey>::const_iterator itr =
        lock_targets.begin(), end = lock_targets.end();
        itr != end; ++itr) {
      // internal property check
      if (GetInternalPropertyAccessor(itr->GetPropertyKey()) != NULL) {
        return SENSCORD_STATUS_FAIL(
            kStatusBlockCore, Status::kCauseInvalidArgument,
            "this property does not support locks: %s",
            itr->GetPropertyKey().c_str());
      }
      // duplicate check (removed additional info)
      if (itr->GetPropertyKey() != itr->GetFullKey()) {
        std::set<std::string>::const_iterator found =
            keys.find(itr->GetPropertyKey());
        if (found != keys.end()) {
          return SENSCORD_STATUS_FAIL(
              kStatusBlockCore, Status::kCauseInvalidArgument,
              "duplicate key: %s/%s", itr->GetFullKey().c_str(),
              found->c_str());
        }
      }
    }
  }
  Status status = adapter_->LockProperty(
      config_.address.port_type, config_.address.port_id,
      this, lock_targets, timeout_msec, lock_resource);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unlock to access properties.
 * @return Status object.
 */
Status StreamCore::UnlockProperty() {
  if (lock_resource_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not locked");
  }
  Status status = UnlockProperty(lock_resource_);
  if (status.ok()) {
    lock_resource_ = NULL;
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unlock to access properties.
 * @param[in] (lock_resource) Locked properties resource.
 * @return Status object.
 */
Status StreamCore::UnlockProperty(PropertyLockResource* lock_resource) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  if (lock_resource == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "lock_resource is NULL");
  }

  Status status = adapter_->UnlockProperty(
      config_.address.port_type, config_.address.port_id, this, lock_resource);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Register the callback for frame reached.
 * @param[in] (callback) Function pointer.
 * @param[in] (private_data) Private data with callback.
 * @return Status object.
 */
Status StreamCore::RegisterFrameCallback(
    const Stream::OnFrameReceivedCallback callback,
    void* private_data) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (callback == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "callback is null");
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  // fixed callback strategy
  CallbackStrategy strategy = kCallbackStrategyOrder;

  bool is_running = (GetLocalState() == kStreamLocalStateRunning);

  util::AutoLock callback_lock(&mutex_callback_frame_);
  // Create next observer and check parameter.
  FrameObserver* next_observer = CreateFrameObserver(strategy);
  Status status = SetupFrameObserver(next_observer, callback, private_data,
      is_running);
  if (!status.ok()) {
    delete next_observer;
    return SENSCORD_STATUS_TRACE(status);
  }

  if (frame_observer_ != NULL) {
    UnregisterFrameCallback();
  }
  frame_observer_ = next_observer;
  frame_notifier_.AddObserver(frame_observer_);
  return Status::OK();
}

/**
 * @brief Unregister the callback for frame reached.
 * @return Status object.
 */
Status StreamCore::UnregisterFrameCallback() {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  FrameObserver* observer = NULL;
  {
    util::AutoLock callback_lock(&mutex_callback_frame_);
    if (frame_observer_ != NULL) {
      observer = frame_observer_;
      frame_observer_ = NULL;
    }
  }
  if (observer != NULL) {
    frame_notifier_.RemoveObserver(observer);

    observer->Stop();
    observer->Exit();
    delete observer;
  }
  return Status::OK();
}

/**
 * @brief Register the callback for event receiving.
 * @param[in] (event_type) Event type to receive.
 * @param[in] (callback) Function pointer.
 * @param[in] (private_data) Private data with callback.
 * @return Status object.
 */
Status StreamCore::RegisterEventCallback(
    const std::string& event_type,
    const OnEventReceivedCallback callback,
    void* private_data) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (callback == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "callback is null");
  }

  EventObserver::SetupParameter param = {};
  param.stream = this;
  param.event_type = event_type;
  param.callback = callback;
  param.private_data = private_data;

  Status status = RegisterEventCallbackCore(param);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @deprecated
 * @brief Register the callback for event receiving.
 * @param[in] (event_type) Event type to receive.
 * @param[in] (callback) Function pointer.
 * @param[in] (private_data) Private data with callback.
 * @return Status object.
 */
Status StreamCore::RegisterEventCallback(
    const std::string& event_type,
    const OnEventReceivedCallbackOld callback,
    void* private_data) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (callback == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "callback is null");
  }

  EventObserver::SetupParameter param = {};
  param.stream = this;
  param.event_type = event_type;
  param.callback_old = callback;
  param.private_data = private_data;

  Status status = RegisterEventCallbackCore(param);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Register the event callback.
 * @param[in] (event_type) Registered event type.
 * @param[in] (setup_param) Setup parameters.
 * @return Status object.
 */
Status StreamCore::RegisterEventCallbackCore(
    const EventObserver::SetupParameter& setup_param) {
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  const std::string& event_type = setup_param.event_type;
  if (event_type.empty()) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "event_type is empty");
  }
  util::AutoLock lock(&mutex_callback_event_);
#ifdef SENSCORD_SERVER
  // send request to client
  if ((config_.address.port_type == kPortTypeClient) &&
      (event_observer_.find(event_type) == event_observer_.end())) {
    RegisterEventProperty property = {};
    property.event_type = event_type;
    Status status = SetProperty(kRegisterEventPropertyKey, &property);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
  }
#endif  // SENSCORD_SERVER
  // fixed callback strategy
  CallbackStrategy strategy = kCallbackStrategyOrder;

  EventObserver* observer = CreateEventObserver(strategy);
  {
    Status status = observer->Init(setup_param);
    if (!status.ok()) {
      delete observer;
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  UnregisterEventCallbackCore(event_type);
  event_observer_.insert(std::make_pair(event_type, observer));
  event_notifier_.AddObserver(observer);
  return Status::OK();
}

/**
 * @brief Unregister the event callback.
 * @param[in] (event_type) Registered event type.
 * @return Status object.
 */
Status StreamCore::UnregisterEventCallback(const std::string& event_type) {
  StreamFunctionLock func_lock(lock_manager_, kStreamFunctionTypeInternal);
  if (!func_lock.GetStatus().ok()) {
    return SENSCORD_STATUS_TRACE(func_lock.GetStatus());
  }
  if (GetLocalState() < kStreamLocalStateInit) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
#ifdef SENSCORD_SERVER
  // send request to client
  if ((config_.address.port_type == kPortTypeClient) &&
      (event_observer_.find(event_type) != event_observer_.end())) {
    RegisterEventProperty property = {};
    property.event_type = event_type;
    Status status = SetProperty(kUnregisterEventPropertyKey, &property);
    if (!status.ok()) {
      return SENSCORD_STATUS_TRACE(status);
    }
  }
#endif  // SENSCORD_SERVER
  Status status = UnregisterEventCallbackCore(event_type);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Unregister the event callback main process.
 * @param[in] (event_type) Registered event type.
 * @return Status object.
 */
Status StreamCore::UnregisterEventCallbackCore(const std::string& event_type) {
  util::AutoLock lock(&mutex_callback_event_);
  std::map<std::string, EventObserver*>::iterator itr =
      event_observer_.find(event_type);
  if (itr == event_observer_.end()) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "no registered event type: %s",
        event_type.c_str());
  }
  EventObserver* observer = itr->second;
  event_observer_.erase(itr);
  if (observer == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "observer is null");
  }
  event_notifier_.RemoveObserver(observer);
  observer->Exit();
  delete observer;
  return Status::OK();
}

/**
 * @brief Unregister the event callback of all.
 * @return Status object.
 */
Status StreamCore::UnregisterEventCallbackAll() {
  util::AutoLock lock(&mutex_callback_event_);
  while (!event_observer_.empty()) {
    std::map<std::string, EventObserver*>::iterator itr =
        event_observer_.begin();
    Status status = UnregisterEventCallback(itr->first);
    if (!status.ok()) {
      SENSCORD_LOG_WARNING("%s", status.ToString().c_str());
      return SENSCORD_STATUS_TRACE(status);
    }
  }
  return Status::OK();
}

/**
 * @brief Returns whether this this instance can be released.
 * @return true is releasable.
 */
bool StreamCore::IsReleasable() {
  if (GetLocalState() == kStreamLocalStateNotInit &&
      !lock_manager_->IsAnotherThreadAccessing()) {
    return true;
  }
  return false;
}

/**
 * @brief Wait until this instance can be released.
 */
void StreamCore::WaitForReleasable() {
  lock_manager_->WaitAllAccessDone();
}

/**
 * @brief Get this stream's type.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) The type of stream.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    StreamTypeProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  property->type = config_.radical_address.port_type;
  return Status::OK();
}

/**
 * @brief Set this stream's type.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const StreamTypeProperty* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}

/**
 * @brief Get this stream's key.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) The key of stream.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    StreamKeyProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  property->stream_key = config_.stream_key;
  return Status::OK();
}

/**
 * @brief Set this stream's key.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const StreamKeyProperty* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}

/**
 * @brief Get the current state of this stream.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) The current state of stream.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    StreamStateProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }

#ifdef AITRIOS_SPECIFIC
  // Get state from server via adapter
#ifdef SENSCORD_SERIALIZE
  void* output_property = NULL;
  size_t output_property_size = 0;
  Status status = adapter_->GetSerializedProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, NULL, 0, &output_property, &output_property_size);
  if (status.ok()) {
    serialize::Decoder decoder(output_property, output_property_size);
    status = decoder.Pop(*property);
    adapter_->ReleaseSerializedProperty(
        config_.address.port_type, config_.address.port_id,
        property_key, output_property, output_property_size);
  }
  return SENSCORD_STATUS_TRACE(status);
#else
  Status status = adapter_->GetProperty(
      config_.address.port_type, config_.address.port_id,
      this, property_key, property);
  return SENSCORD_STATUS_TRACE(status);
#endif  // SENSCORD_SERIALIZE
#else
  StreamLocalState state = GetLocalState();
  // Translate from local state to public state.
  switch (state) {
    case kStreamLocalStateReady:
      property->state = kStreamStateReady;
      break;

    case kStreamLocalStateRunning:
      property->state = kStreamStateRunning;
      break;

    default:
      property->state = kStreamStateUndefined;
      break;
  }
  return Status::OK();
#endif  // AITRIOS_SPECIFIC
}

/**
 * @brief Set the current state of this stream.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const StreamStateProperty* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}


/**
 * @brief Get this stream's frame buffering settings.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Frame buffering settings.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    FrameBuffering* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  *property = config_.frame_buffering;
  return Status::OK();
}

/**
 * @brief Set this stream's frame buffering settings.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const FrameBuffering* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}

/**
 * @brief Get the current status of frame buffering.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Frame buffering current status.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    CurrentFrameNumProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager is null");
  }
  Status status = frame_manager_->GetFrameBufferInfo(NULL,
      &property->arrived_number, &property->received_number);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the current status of frame buffering.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const CurrentFrameNumProperty* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}

#ifdef SENSCORD_RECORDER
/**
 * @brief Get the recording settings.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Current recording state.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key, RecordProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  recorder_.GetState(property);
  return Status::OK();
}

/**
 * @brief Set the recording property.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) New recording state.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& property_key, const RecordProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }

  Status status;
  if (property->enabled) {
    // stop -> record
    StreamLocalState state = GetLocalState();
    if (state != kStreamLocalStateRunning) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation, "not running state");
    }
    status = recorder_.Start(*property);
    SENSCORD_STATUS_TRACE(status);
  } else {
    // record -> stop
    status = recorder_.Stop();
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Get the recordable formats.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) List of recordable formats.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    RecorderListProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }

  Status status = recorder_.GetRecordableFormats(&property->formats);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the recordable formats.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& /* property_key */,
    const RecorderListProperty* /* property */) {
  // unsupported.
  return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseNotSupported,
      "not supported property");
}
#endif  // SENSCORD_RECORDER

/**
 * @brief Get the current user data.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Current user data.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    UserDataProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager is null");
  }

  FrameUserData* user_data = NULL;
  Status status = frame_manager_->GetUserData(&user_data);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // copy user data
  if (user_data->data_size > 0) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(user_data->data_address);
    property->data.reserve(user_data->data_size);
    property->data.assign(ptr, ptr + user_data->data_size);
  } else {
    property->data.clear();
  }

  return Status::OK();
}

/**
 * @brief Set the user data.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) New user data.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& property_key,
    const UserDataProperty* property) {
  FrameUserData user_data;
  user_data.data_size = 0;
  user_data.data_address = 0;
  if (property != NULL) {
    const size_t data_size = property->data.size();
    if (data_size > kUserDataPropertyMaxSize) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidArgument, "over the max size of userdata");
    }
    if (data_size > 0) {
      user_data.data_size = data_size;
      user_data.data_address = reinterpret_cast<uintptr_t>(&property->data[0]);
    }
  }
  Status status = adapter_->SetUserData(
      config_.address.port_type, config_.address.port_id, user_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the mask of the channels.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Current mask.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    ChannelMaskProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }

  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager was deleted");
  }
  Status status = frame_manager_->GetChannelMask(&property->channels);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the mask of the channels.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) New mask.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& property_key,
    const ChannelMaskProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }

  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager was deleted");
  }
  Status status = frame_manager_->SetChannelMask(property->channels);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Get the skip rate of the frame.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Current skip rate.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    SkipFrameProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager was deleted");
  }
  Status status = frame_manager_->GetSkipRate(&property->rate);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set the skip rate of the frame.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) New skip rate.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& property_key,
    const SkipFrameProperty* property) {
  if (property == NULL) {
    static const SkipFrameProperty disableSkip = {1};
    property = &disableSkip;
  }
  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager was deleted");
  }
  Status status = frame_manager_->SetSkipRate(property->rate);
  return SENSCORD_STATUS_TRACE(status);
}

#ifdef SENSCORD_SERVER
/**
 * @brief Get the frame extension.
 * @param[in] (property_key) Key of property to get.
 * @param[out] (property) Frame extension.
 * @return Status object.
 */
Status StreamCore::Get(
    const std::string& property_key,
    FrameExtensionProperty* property) {
  return SENSCORD_STATUS_FAIL(kStatusBlockCore,
      Status::kCauseNotSupported, "property not supported");
}

/**
 * @brief Set the frame extension.
 * @param[in] (property_key) Key of property to set.
 * @param[in] (property) Frame extension.
 * @return Status object.
 */
Status StreamCore::Set(
    const std::string& property_key,
    const FrameExtensionProperty* property) {
  if (property == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "property is null");
  }
  if (property->disabled) {
    delete frame_extension_;
    frame_extension_ = NULL;
  }
  return Status::OK();
}
#endif

/**
 * @brief Get local state.
 * @return Stream local state.
 */
StreamLocalState StreamCore::GetLocalState() const {
  return lock_manager_->GetStreamLocalState();
}

/**
 * @brief Get local state.
 * @param[out] (is_changing) State changing progress
 * @return Stream local state.
 */
StreamLocalState StreamCore::GetLocalState(bool* is_changing) const {
  return lock_manager_->GetStreamLocalState(is_changing);
}

/**
 * @brief Begin the state change.
 * @param[in] (state) New state to set.
 * @return Status object.
 */
Status StreamCore::BeginLocalStateChange(StreamLocalState state) {
  return lock_manager_->BeginStateChange(state);
}

/**
 * @brief Commit the state change.
 */
void StreamCore::CommitLocalStateChange() {
  lock_manager_->CommitStateChange();
}

/**
 * @brief Cancel the state change.
 */
void StreamCore::CancelLocalStateChange() {
  lock_manager_->CancelStateChange();
}

/**
 * @brief Notify to release frame (for only FrameManager).
 * @param[in] (frameinfo) Frame informations to release.
 * @param[in] (referenced_channel_ids) List of referenced channel IDs.
 * @return Status object.
 */
Status StreamCore::ReleaseFrameInfo(
    const FrameInfo& frameinfo,
    const std::vector<uint32_t>& referenced_channel_ids) {
  Status status;
  if (adapter_ != NULL) {
    status = adapter_->ReleaseFrame(config_.address.port_type,
        config_.address.port_id, this, frameinfo, referenced_channel_ids);
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Send the multiple frames information from Component.
 * @param[in] (frames) List of frame information to send.
 * @param[in] (sent_time) Time when frame was sent.
 * @param[out] (dropped_frames) List of dropped frames.
 * @return Status object.
 */
Status StreamCore::SendFrames(const std::vector<FrameInfo>& frames,
                              uint64_t sent_time,
                              std::vector<const FrameInfo*>* dropped_frames) {
  if (dropped_frames == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "dropped_frames is null");
  }
  util::AutoLock frame_lock(&mutex_frame_);
  if (!IsSendableState() ||
     (frame_manager_ == NULL)) {
    for (std::vector<FrameInfo>::const_iterator itr = frames.begin(),
        end = frames.end(); itr != end; ++itr) {
      dropped_frames->push_back(&(*itr));
    }
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "invalid state");
  }
  // Register multiple frames.
  for (std::vector<FrameInfo>::const_iterator itr = frames.begin(),
      end = frames.end(); itr != end; ++itr) {
    Status status = frame_manager_->Set(*itr, sent_time);
    if (!status.ok()) {
      dropped_frames->push_back(&(*itr));
    }
  }
  if (!dropped_frames->empty()) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseBusy, "frames dropped.");
  }
  return Status::OK();
}

/**
 * @brief Judge the stream state is sendable the frame.
 * @return if sendable then return true.
 */
bool StreamCore::IsSendableState() const {
  bool is_changing = false;
  StreamLocalState state = GetLocalState(&is_changing);
  if ((state == kStreamLocalStateRunning) && (!is_changing)) {
    return true;
  }
  return false;
}

/**
 * @brief Send the event from Component.
 * @param[in] (event) Event type to send.
 * @param[in] (args) Event argument.
 * @return Status object.
 */
Status StreamCore::SendEvent(
    const std::string& event, const EventArgument& args) {
  EventInfo info;
  info.type = event;
  info.argument = args;
  Status status = event_notifier_.NotifyObservers(&info);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Set user data to frame manager.
 * @param[in] (user_data) New user data.
 * @return Status object.
 */
Status StreamCore::SetUserData(const FrameUserData& user_data) {
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager is null");
  }
  Status status = frame_manager_->SetUserData(user_data);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Wakeup wait frame.
 * @param[in] (mutex) Mutex object to lock.
 * @return true is successful, false is failed.
 */
bool StreamCore::WakeupWaitFrame(util::Mutex* mutex) {
  if (mutex == NULL) {
    return false;
  }
  util::AutoLock lock(mutex);
  if (osal::OSBroadcastCond(cond_frame_) < 0) {
    return false;
  }
  return true;
}

/**
 * @brief Create frame manager.
 * @param[in] (config) Frame buffering configuration.
 * @return Status object.
 */
Status StreamCore::CreateFrameManager(const FrameBuffering& config) {
  frame_manager_ = FrameManagerFactory::CreateInstance(config);
  if (frame_manager_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "frame manager is null");
  }

  Status status = frame_manager_->Init(config.num, this);
  if (!status.ok()) {
    FrameManagerFactory::DestroyInstance(frame_manager_);
    frame_manager_ = NULL;
    SENSCORD_STATUS_TRACE(status);
  }
  return status;
}

/**
 * @brief Destroy frame manager and relative property accesser.
 * @return Status object.
 */
Status StreamCore::DestroyFrameManager() {
  util::AutoLock lock(&mutex_frame_);
  if (frame_manager_ != NULL) {
    FrameManagerFactory::DestroyInstance(frame_manager_);
    frame_manager_ = NULL;
  }
  return Status::OK();
}

/**
 * @brief Notify frame arrived from FrameManager.
 * @param[in] (frameinfo) Arrived frame information.
 * @return Status object.
 */
Status StreamCore::FrameArrived(const FrameInfo& frameinfo) {
  if (IsSendableState()) {
    WakeupWaitFrame(&mutex_frame_);
    util::AutoLock lock(&mutex_callback_frame_);
    frame_notifier_.NotifyObservers(&frameinfo);
  }
  return Status::OK();
}

/**
 * @brief Get the shared property accessor related this stream.
 * @param[in] (property_key) Search by property key.
 * @return Property accessor. null is not found.
 */
PropertyAccessor* StreamCore::GetSharedPropertyAccessor(
    const std::string& property_key) const {
  const PropertyAccessorList& list = property_list_shared_;
  util::AutoLock lock(&mutex_property_);
  PropertyAccessorList::const_iterator itr = list.find(
      senscord::PropertyUtils::GetKey(property_key));
  if (itr != list.end()) {
    return itr->second;
  }
  return NULL;  // not found
}

/**
 * @brief Get the internal property accessor related this stream.
 * @param[in] (property_key) Search by property key.
 * @return Property accessor. null is not found.
 */
PropertyAccessor* StreamCore::GetInternalPropertyAccessor(
    const std::string& property_key) const {
  const PropertyAccessorList& list = property_list_;
  util::AutoLock lock(&mutex_property_);
  PropertyAccessorList::const_iterator itr = list.find(
      senscord::PropertyUtils::GetKey(property_key));
  if (itr != list.end()) {
    return itr->second;
  }
  return NULL;  // not found
}

/**
 * @brief Register the shared property accessor.
 * @param[in] (accessor) Property accessor to register.
 * @return Status object.
 */
Status StreamCore::RegisterSharedPropertyAccessor(
    PropertyAccessor* accessor) {
  util::AutoLock lock(&mutex_property_);
  PropertyAccessorList& list = property_list_shared_;
  bool ret = list.insert(std::make_pair(accessor->GetKey(), accessor)).second;
  if (!ret) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAlreadyExists,
        "Shared Property is already registered: '%s'",
        accessor->GetKey().c_str());
  }
  return Status::OK();
}

/**
 * @brief Register the internal property accessor.
 * @param[in] (accessor) Property accessor to register.
 * @return Status object.
 */
Status StreamCore::RegisterInternalPropertyAccessor(
    PropertyAccessor* accessor) {
  util::AutoLock lock(&mutex_property_);
  PropertyAccessorList& list = property_list_;
  bool ret = list.insert(std::make_pair(accessor->GetKey(), accessor)).second;
  if (!ret) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAlreadyExists,
        "Internal Property is already registered: '%s'",
        accessor->GetKey().c_str());
  }
  return Status::OK();
}

/**
 * @brief Create frame observer by strategy.
 * @param[in] (strategy) Observing strategy.
 * @return New frame observer.
 */
FrameObserver* StreamCore::CreateFrameObserver(CallbackStrategy strategy) {
  return new FrameObserverOrder();
}

/**
 * @brief Setup frame observer.
 * @param[in] (observer) Frame observer.
 * @param[in] (callback) Setup callback.
 * @param[in] (private_data) Setup private data.
 * @param[in] (is_running) Run new observer.
 * @return Status object.
 */
Status StreamCore::SetupFrameObserver(
    FrameObserver* observer,
    const Stream::OnFrameReceivedCallback callback,
    void* private_data,
    bool is_running) {
  if (observer == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "observer is null");
  }
  FrameObserver::SetupParameter param;
  param.stream = this;
  param.callback = callback;
  param.private_data = private_data;
  Status status = observer->Init(param);
  if ((status.ok()) && is_running) {
    status = observer->Start();
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      observer->Exit();
    }
  }
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Create event observer by strategy.
 * @param[in] (strategy) Observing strategy.
 * @return New event observer.
 */
EventObserver* StreamCore::CreateEventObserver(CallbackStrategy strategy) {
  return new EventObserverOrder();
}

/**
 * @brief Start frame observing.
 * @return Status object.
 */
Status StreamCore::StartFrameObserver() {
  util::AutoLock lock(&mutex_callback_frame_);
  if (frame_observer_) {
    return frame_observer_->Start();
  }
  return Status::OK();
}

/**
 * @brief Stop frame observing.
 * @return Status object.
 */
Status StreamCore::StopFrameObserver() {
  util::AutoLock lock(&mutex_callback_frame_);
  if (frame_observer_) {
    return frame_observer_->Stop();
  }
  return Status::OK();
}

/**
 * @brief Open the stream extension.
 * @return Status object.
 */
Status StreamCore::OpenStreamExtension() {
  Status status;
  std::vector<const ExtensionLibrary*> libraries =
      ExtensionManager::GetInstance()->GetStreamExtensionLibraries(
          config_.stream_key);
  for (std::vector<const ExtensionLibrary*>::const_iterator
      itr = libraries.begin(), end = libraries.end(); itr != end; ++itr) {
    const ExtensionLibrary* library = *itr;
    StreamExtension* stream_extension =
        library->CreateInstance<StreamExtension>("StreamExtension");
    if (stream_extension != NULL) {
      std::map<std::string, MemoryAllocator*> allocators;
      status = GetAllocatorsByStreamExtension(
          library->GetLibraryName(), &allocators);
      SENSCORD_STATUS_TRACE(status);
      if (!status.ok()) {
        delete stream_extension;
        break;
      }
      FrameExtensionAdapter* adapter = new FrameExtensionAdapter();
      stream_extension->Init(this, allocators, adapter);

      std::map<std::string, std::string> arguments;
      GetArgumentsByStreamExtension(library->GetLibraryName(), &arguments);
      status = stream_extension->Open(arguments);
      SENSCORD_STATUS_TRACE(status);
      if (!status.ok()) {
        delete adapter;
        delete stream_extension;
        break;
      }
      stream_extensions_.push_back(stream_extension);

      if (adapter->IsRegistered()) {
        delete frame_extension_;
        frame_extension_ = adapter;
      } else {
        delete adapter;
      }
    }
  }

#ifdef SENSCORD_SERVER
  if (frame_extension_ && config_.address.port_type == kPortTypeClient &&
      status.ok()) {
    FrameExtensionType type = frame_extension_->GetFrameExtensionType();
    if (type == kFrameExtensionNormal) {
      // disable server setting
      FrameExtensionProperty property = {};
      property.disabled = true;
      status = SetProperty(kFrameExtensionPropertyKey, &property);
      SENSCORD_STATUS_TRACE(status);
    } else if (type == kFrameExtensionShared) {
      // disable frame extension
      delete frame_extension_;
      frame_extension_ = NULL;
    }
  }
#endif
  return status;
}

/**
 * @brief Get the allocators by stream extension
 * @param[in] (library_name) Stream extension library name.
 * @param[out] (allocators) Allocators.
 */
Status StreamCore::GetAllocatorsByStreamExtension(
    const std::string& library_name,
    std::map<std::string, MemoryAllocator*>* allocators) {
  MemoryManager* memory_manager = MemoryManager::GetInstance();
  for (std::vector<ExtensionSetting>::const_iterator
      extension = config_.extensions.begin(),
      ex_end = config_.extensions.end(); extension != ex_end; ++extension) {
    if (extension->library_name == library_name) {
      allocators->clear();    // overwrite later
      for (std::map<std::string, std::string>::const_iterator
          allocator_itr = extension->allocators.begin(),
          allocator_end = extension->allocators.end();
          allocator_itr != allocator_end; ++allocator_itr) {
        MemoryAllocator* allocator = NULL;
        Status status = memory_manager->GetAllocator(
            allocator_itr->second, &allocator);
        if (!status.ok()) {
          return SENSCORD_STATUS_TRACE(status);
        }
        allocators->insert(std::make_pair(allocator_itr->first, allocator));
      }
    }
  }
  if (allocators->empty()) {
    MemoryAllocator* allocator = NULL;
    Status status = memory_manager->GetAllocator(
        kAllocatorNameDefault, &allocator);
    if (status.ok()) {
      allocators->insert(std::make_pair(kAllocatorNameDefault, allocator));
    } else {
      SENSCORD_LOG_WARNING("%s", status.ToString().c_str());
    }
  }
  return Status::OK();
}

/**
 * @brief Get the arguments by stream extension
 * @param[in] (library_name) Stream extension library name.
 * @param[out] (arguments) Arguments.
 */
void StreamCore::GetArgumentsByStreamExtension(
    const std::string& library_name,
    std::map<std::string, std::string>* arguments) {
  for (std::vector<ExtensionSetting>::const_iterator
      extension = config_.extensions.begin(),
      ex_end = config_.extensions.end(); extension != ex_end; ++extension) {
    if (extension->library_name == library_name) {
      *arguments = extension->arguments;    // overwrite later
    }
  }
}

/**
 * @brief Close the stream extension.
 * @param[in] (stop_on_error) If true, stop on error.
 * @return Status object.
 */
Status StreamCore::CloseStreamExtension(bool stop_on_error) {
  Status result;
  while (!stream_extensions_.empty()) {
    StreamExtension* stream_extension = stream_extensions_.back();
    Status status = stream_extension->Close();
    if (!status.ok()) {
      result = SENSCORD_STATUS_TRACE(status);
      if (stop_on_error) {
        break;
      }
    }
    delete stream_extension;
    stream_extensions_.pop_back();
  }
  return result;
}

}   // namespace senscord
