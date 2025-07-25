/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "player_frame_file_manager.h"

#include <inttypes.h>
#include <stdint.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "./player_component_types.h"
#include "./player_component_util.h"
#include "./player_autolock.h"
#include "senscord/logger.h"

namespace {
  const size_t kMaximumQueueSize = 10;
  const uint64_t kReadSleepCoefficient = 5;  // Half the size queue size
  const char* kModuleName = "player_frame_file_manager";
}

/**
 * @brief Thread for frame writing.
 * @param[in] (arg) Adapter for frame recorder.
 */
static senscord::osal::OSThreadResult WorkerThreadEntry(void* arg) {
  PlayerFrameFileManager* adapter =
      reinterpret_cast<PlayerFrameFileManager*>(arg);
  if (adapter != NULL) {
    adapter->ReadFrameThread();
  }
  return 0;
}

/**
 * @brief Constructor.
 */
PlayerFrameFileManager::PlayerFrameFileManager(
    senscord::MemoryAllocator* allocator)
    : target_path_(),
      read_sleep_time_(0),
      allocator_(allocator),
      read_thread_(NULL),
      is_started_(false),
      mutex_started_(NULL),
      mutex_frame_queue_(NULL),
      cond_frame_buffering_(NULL),
      raw_index_(),
      total_frames_(),
      play_frames_(),
      channel_list_(),
      raw_index_path_(),
      start_position_() {
  total_frames_.clear();
  play_frames_.clear();
  senscord::osal::OSCreateMutex(&mutex_started_);
  senscord::osal::OSCreateMutex(&mutex_frame_queue_);
  senscord::osal::OSCreateCond(&cond_frame_buffering_);
}

/**
 * @brief Destructor.
 */
PlayerFrameFileManager::~PlayerFrameFileManager() {
  ClearRawIndex();
  ClearChannel();
  senscord::osal::OSDestroyMutex(mutex_started_);
  mutex_started_ = NULL;
  senscord::osal::OSDestroyMutex(mutex_frame_queue_);
  mutex_frame_queue_ = NULL;
  senscord::osal::OSDestroyCond(cond_frame_buffering_);
  cond_frame_buffering_ = NULL;
}

/**
 * @brief Setup frame file manager
 * @param[in] (play_property) The play property
 * @param[in] (channels) The channel information of info.xml
 * @param[in] (buffer_size) The size of raw index buffer.
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::SetupFrameFileManager(
    const std::string& target_path,
    const InfoXmlChannelList& channels,
    const size_t buffer_size) {
  senscord::Status status;

  status = ReadChannelProperty(target_path, channels);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  status = ReadRawIndex(target_path, channels, buffer_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // apply (use only in thread)
  target_path_ = target_path;

  return senscord::Status::OK();
}

/**
 * @brief Adjust frame queue by timestamp (Discard old frame from the queue)
 * @param[in] (sent_time) The timestamp of sync position.
 * @note Use only synchronized playback.
 */
void PlayerFrameFileManager::AdjustFrameQueueByTimestamp(uint64_t sent_time) {
  // A zero in this count indicates that all frames have been checked.
  size_t check_counter = play_frames_.size();

  while (check_counter--) {
    // If the sync position is after the last frame, no adjustment is necessary
    const RecordFrameData& last_frame = play_frames_.back();
    if (last_frame.sent_time <= sent_time) {
      break;
    }

    // If queue is empty, wait to add it to the queue.
    WaitFrameBuffering();

    player::AutoLock autolock(mutex_frame_queue_);
    while (!frame_queue_.empty()) {
      PlayFrameQueue::iterator itr = frame_queue_.begin();
      // Is the adjustment complete.
      if (sent_time <= itr->first) {
        check_counter = 0;
        break;
      }

      // Discard old frame data from the queue
      for (size_t i = 0; i < itr->second->frame_info.channels.size(); ++i) {
        allocator_->Free(itr->second->frame_info.channels[i].data_memory);
      }
      delete itr->second;
      frame_queue_.erase(itr);
    }  // while (!frame_queue_.empty())
  }  // while (check_counter--)
}

/**
 * @brief Get the frame from queue
 * @param[out] (frame) frame data
 * @return status object
 */
senscord::Status PlayerFrameFileManager::GetFrame(PlayFrame** frame) {
  player::AutoLock autolock(mutex_frame_queue_);
  if (frame_queue_.empty()) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseAborted, "frame_queue_ is empty");
  }
  PlayFrameQueue::iterator itr = frame_queue_.begin();
  *frame = itr->second;
  frame_queue_.erase(itr);

  return senscord::Status::OK();
}

/**
 * @brief Read ChannelPropertyFile.
 * @param[in] (target_path) Target directory for read to channel data.
 * @param[in] (channels) The channel information of info.xml
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::ReadChannelProperty(
    const std::string& target_path, const InfoXmlChannelList& channels) {
  senscord::Status status;
  {
    InfoXmlChannelList::const_iterator itr = channels.begin();
    InfoXmlChannelList::const_iterator end = channels.end();
    for (; itr != end; ++itr) {
      const uint32_t& channel_id = itr->first;
      const InfoXmlChannelParameter& channel = itr->second;

      // Create a manage channel
      PlayerComponentChannelData* data = new PlayerComponentChannelData;
      data->type = channel.rawdata_type;
      data->description = channel.description;
      if (!channel.mask) {
        status = ReadChannelPropertyFile(
            target_path, channel_id, &(data->property_list));
        if (!status.ok()) {
          SENSCORD_LOG_WARNING(
              "Failed to read the channel property: id=%" PRIu32 ", ret=%s",
              channel_id, status.ToString().c_str());
          // Continue playback without the channel property
        }
      }

      // Append a channel to manage list
      channel_list_.insert(std::make_pair(channel_id, data));
    }
  }

  return senscord::Status::OK();
}

/**
 * @brief Clear PlayerComponentChannelData.
 */
void PlayerFrameFileManager::ClearChannel() {
  std::map<uint32_t, PlayerComponentChannelData*>::const_iterator itr;
  for (itr = channel_list_.begin(); itr != channel_list_.end(); ++itr) {
    delete itr->second;
  }
  channel_list_.clear();
}

/**
 * @brief read channel property file "channel_0xHHHHHHHH/properties.dat"
 * and create PlayerComponentChanData::PlayerComponentPropertyListBySeqNo
 * property_list
 * @param[in] (target_path) Target directory for read to channel data.
 * @param[in] (channel_id) channel_id
 * @param[in] (p_list) data to be created
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::ReadChannelPropertyFile(
    const std::string& target_path, uint32_t channel_id,
    PlayerComponentPropertyListBySeqNo* p_list) {
  std::string full_path;
  std::string path1;
  senscord::Status status;

  senscord::RecordUtility::GetChannelPropertiesFilePath(channel_id, &path1);
  full_path = target_path + senscord::osal::kDirectoryDelimiter + path1;

  if (p_list == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseAborted, "p_list is NULL");
  }

  std::vector<uint8_t> read_buffer;
  status = player::FileReadAllData(full_path.c_str(), &read_buffer);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  senscord::serialize::Decoder decoder(
      read_buffer.data(), read_buffer.size());
  while (1) {
    senscord::ChannelPropertiesForRecord curr_record = {};
    status = decoder.Pop(curr_record);
    if (!status.ok()) {
      break;
    }
    p_list->insert(std::make_pair(
        curr_record.sequence_number, curr_record.properties));
  }

  return senscord::Status::OK();
}

/**
 * @brief Find PlayerComponentChannelData.
 * @param[in] (channel_id) Target channel ID.
 * @return Channel data.
 */
PlayerComponentChannelData* PlayerFrameFileManager::GetChannelData(
    uint32_t channel_id) {
  std::map<uint32_t, PlayerComponentChannelData*>::iterator itr;
  for (itr = channel_list_.begin(); itr != channel_list_.end(); ++itr) {
    if (itr->first == channel_id) {
      return itr->second;
    }
  }
  return NULL;
}

/**
 * @brief find channel property by sequence_number
 * @param[in] (channel_id) channel_id
 * @param[in] (sequence_number) sequence_number
 * @return The list of binary properties
 */
const BinaryPropertyList* PlayerFrameFileManager::GetChannelPropertyList(
    uint32_t channel_id, uint64_t sequence_number) {
  PlayerComponentChannelData* channel_data = GetChannelData(channel_id);
  if (channel_data == NULL) {
    return NULL;
  }

  PlayerComponentPropertyListBySeqNo::const_iterator itr =
      channel_data->property_list.find(sequence_number);
  if (itr != channel_data->property_list.end()) {
    return &itr->second;
  }
  return NULL;
}

/**
 * @brief Read raw index from record file.
 * @param[in] (target_path) Target directory for read to raw index.
 * @param[in] (channels) The channel information of info.xml
 * @param[in] (buffer_size) The size of raw index buffer.
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::ReadRawIndex(
    const std::string& target_path, const InfoXmlChannelList& channels,
    const size_t buffer_size) {
  senscord::Status status;
  std::string file_name;
  senscord::RecordUtility::GetRawIndexFilePath(&file_name);
  std::string file_path =
      target_path + senscord::osal::kDirectoryDelimiter + file_name;

  // open
  senscord::osal::OSFile* file = NULL;
  size_t file_size = 0;
  status = player::OpenFile(file_path, &file, &file_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // allocate read buffer
  uint8_t* read_buffer;
  if (file_size > buffer_size) {
    read_buffer = new uint8_t[buffer_size];
  } else {
    read_buffer = new uint8_t[file_size];
  }

  size_t file_offset = 0;
  while (1) {
    // read size
    size_t read_size = file_size - file_offset;
    if (read_size > buffer_size) {
      read_size = buffer_size;
    }

    // seek and read
    status = player::ReadFile(file, read_buffer, read_size, file_offset);
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      break;
    }

    // deserialize
    status = DeserializeRawIndexData(
        read_buffer, read_size, &raw_index_, &file_offset);
    if (!status.ok()) {
      SENSCORD_STATUS_TRACE(status);
      break;
    }

    // Deserialization complete
    if (file_offset >= file_size) {
      break;
    }
  }  // while(1)

  delete[] read_buffer;
  senscord::osal::OSFclose(file);
  if (!status.ok()) {
    return status;
  }

  // Apply the file path
  raw_index_path_ = file_path;

  RawIndexList::const_iterator itr = raw_index_.begin();
  RawIndexList::const_iterator end = raw_index_.end();
  for (; itr != end; ++itr) {
    InfoXmlChannelList::const_iterator found = channels.find(itr->channel_id);
    if (found == channels.end()) {
      SENSCORD_LOG_WARNING(
        "Illegal channel_id in the raw index file: id=%" PRIu32,
        itr->channel_id);
      continue;
    }

    // Setting from raw index
    const uint32_t& channel_id = itr->channel_id;
    const senscord::RecordDataType& record_type = itr->record_type;
    const uint64_t& captured_timestamp = itr->captured_timestamp;
    const uint64_t& sequence_number = itr->sequence_number;
    const uint64_t& sent_time = itr->sent_time;
    // Setting from info xml
    const bool& channel_mask = found->second.mask;
    const std::string& rawdata_type = found->second.rawdata_type;

    if (channel_mask) {
      SENSCORD_LOG_DEBUG("Channel to be not read: id=%" PRIu32, channel_id);
      continue;
    }

    if (!total_frames_.empty()) {
      // check same frame
      RecordFrameData* last_frame = &(total_frames_.back());
      if (last_frame->sequence_number == sequence_number) {
        // Append a channel for the last frame.
        RecordRawData rawdata = {};
        rawdata.record_type = record_type;
        rawdata.captured_timestamp = captured_timestamp;
        rawdata.rawdata_type = rawdata_type;
        last_frame->channels.insert(
            std::make_pair(channel_id, rawdata));
        continue;
      }
    }

    RecordFrameData frame = {};
    frame.sequence_number = sequence_number;
    frame.sent_time = sent_time;
    RecordRawData rawdata = {};
    rawdata.record_type = record_type;
    rawdata.captured_timestamp = captured_timestamp;
    rawdata.rawdata_type = rawdata_type;
    frame.channels.insert(
        std::make_pair(channel_id, rawdata));
    total_frames_.push_back(frame);
  }

  return senscord::Status::OK();
}

/**
 * @brief Deserialize the data and apply it to the raw index list.
 * @param[in] (read_buffer) Data buffer read from raw_index.dat.
 * @param[in] (read_size) The size of file read.
 * @param[out] (raw_index_list) List of raw index data.
 * @param[in/out] (file_offset) Deserialized data size of the raw_index.dat.
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::DeserializeRawIndexData(
    const void* read_buffer, size_t read_size, RawIndexList* raw_index_list,
    size_t* file_offset) {
  senscord::Status status;

  size_t buffer_offset = 0;
  senscord::ChannelRawDataForRawIndex record = {};
  senscord::serialize::Decoder decoder(read_buffer, read_size);
  while (1) {
    // pop deserialize data.
    status = decoder.Pop(record);
    if (!status.ok()) {
      break;
    }

    // Apply the deserialize data.
    RawIndexDataWithOffset raw_index = {};
    raw_index.sequence_number = record.sequence_number;
    raw_index.channel_id      = record.channel_id;
    raw_index.captured_timestamp = record.caputured_timestamp;
    raw_index.sent_time       = record.sent_time;
    raw_index.record_type     = record.record_type;

    raw_index.offset = *file_offset + buffer_offset;
    raw_index.size = decoder.GetOffset() - buffer_offset;

    raw_index_list->push_back(raw_index);

    // Update deserialized size
    buffer_offset += raw_index.size;
  }  // while(1)

  // If there is no data in the buffer that can be deserialized
  if (buffer_offset == 0) {
    return SENSCORD_STATUS_FAIL(kModuleName, senscord::Status::kCauseAborted,
        "There is no deserializable data in the buffer: %s",
        status.ToString().c_str());
  }

  // Update file offset
  *file_offset += buffer_offset;

  return senscord::Status::OK();
}

/**
 * @brief Clear raw index.
 */
void PlayerFrameFileManager::ClearRawIndex() {
  raw_index_.clear();
  raw_index_path_.clear();
}

/**
 * @brief Find for raw index by sequence number and channel id
 * @param[in] (sequence_number) The sequence number
 * @param[in] (channel_id) The channel id
 * @return The pointer of raw index data
 */
PlayerFrameFileManager::RawIndexDataWithOffset*
    PlayerFrameFileManager::FindRawIndex(uint64_t sequence_number,
                                         uint32_t channel_id) {
  RawIndexList::iterator itr = raw_index_.begin();
  RawIndexList::iterator end = raw_index_.end();
  for (; itr != end; ++itr) {
    if (itr->sequence_number != sequence_number) {
      continue;
    }
    if (itr->channel_id != channel_id) {
      continue;
    }
    return &(*itr);
  }
  return NULL;
}

/**
 * @brief Set playback range.
 * @param[in] (offset) Start offset.
 * @param[in] (count) Number from start offset.
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::SetPlaybackRange(
    uint32_t offset, uint32_t count) {
  if (offset >= total_frames_.size()) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "Invalid offset value: offset=%" PRIu32 ", total_frame=%" PRIuS,
        offset, total_frames_.size());
  }

  play_frames_.clear();
  uint32_t end_position = 0;
  if ((offset + count) > total_frames_.size()) {
    end_position = static_cast<uint32_t>(total_frames_.size());
  } else {
    if (count != 0) {
      end_position = offset + count;
    } else {
      end_position = static_cast<uint32_t>(total_frames_.size());
    }
  }

  // apply play frames
  play_frames_.assign(
      total_frames_.begin() + offset,
      total_frames_.begin() + end_position);

  return senscord::Status::OK();
}

/**
 * @brief Get the count of playback frames
 */
uint32_t PlayerFrameFileManager::GetPlayCount() {
  return static_cast<uint32_t>(play_frames_.size());
}

/**
 * @brief Get the count of playback frames
 */
uint32_t PlayerFrameFileManager::GetTotalFrameCount() {
  return static_cast<uint32_t>(total_frames_.size());
}

/**
 * @brief Get the sent time of playback frames
 * @param[out] (sent_time_list) The sent time of playback frames
 * @return Status object.
 */
std::vector<uint64_t> PlayerFrameFileManager::GetSentTimeList() {
  std::vector<uint64_t> sent_time_list;
  std::vector<RecordFrameData>::const_iterator itr = play_frames_.begin();
  std::vector<RecordFrameData>::const_iterator end = play_frames_.end();
  for (; itr != end; ++itr) {
    sent_time_list.push_back(itr->sent_time);
  }

  return sent_time_list;
}

/**
 * @brief Get ChannelInfoProperty.
 * @param[out] (prop) The property of channel info.
 */
void PlayerFrameFileManager::GetChannelInfo(
    senscord::ChannelInfoProperty* prop) {
  if (prop != NULL) {
    prop->channels.clear();
    std::map<uint32_t, PlayerComponentChannelData*>::const_iterator itr;
    for (itr = channel_list_.begin(); itr != channel_list_.end(); ++itr) {
      senscord::ChannelInfo ch = {};
      ch.raw_data_type = itr->second->type;
      ch.description = itr->second->description;
      prop->channels.insert(std::make_pair(itr->first, ch));
    }
  }
}

/**
 * @brief Set thread started flag.
 * @param[in] (is_started) The flag of thread started
 */
void PlayerFrameFileManager::SetThreadStarted(bool is_started) {
  // set state
  player::AutoLock autolock(mutex_started_);
  is_started_ = is_started;
}

/**
 * @brief Start a thread
 * @return Status object
 */
senscord::Status PlayerFrameFileManager::StartThreading() {
  int32_t ret = senscord::osal::OSCreateThread(
      &read_thread_, WorkerThreadEntry, this, NULL);
  if (ret != 0) {
    return SENSCORD_STATUS_FAIL(kModuleName,
        senscord::Status::kCauseAborted,
        "failed to OSCreateThread: %" PRIx32, ret);
  }
  return senscord::Status::OK();
}

/**
 * @brief Stop a thread
 */
void PlayerFrameFileManager::StopThreading() {
  // wait for thread stop
  senscord::osal::OSJoinThread(read_thread_, NULL);
}

/**
 * @brief Check the state of the thread
 * @return true: started, false: not started
 */
bool PlayerFrameFileManager::IsThreadStarted() const {
  player::AutoLock autolock(mutex_started_);
  return is_started_;
}

/**
 * @brief Check if the queue size is the maximum
 * @return true: max, false: not max
 */
bool PlayerFrameFileManager::IsFrameQueueMax() const {
  bool is_queue_max = false;
  size_t queue_size = 0;
  {
    player::AutoLock autolock(mutex_frame_queue_);
    queue_size = frame_queue_.size();
  }
  if (queue_size >= kMaximumQueueSize) {
    is_queue_max = true;
  }

  return is_queue_max;
}

/**
 * @brief Check if the queue size is the empty
 * @return true: empty, false: not empty
 */
bool PlayerFrameFileManager::IsFrameQueueEmpty() const {
  player::AutoLock autolock(mutex_frame_queue_);
  return frame_queue_.empty();
}

/**
 * @brief Wait frame buffering.
 */
void PlayerFrameFileManager::WaitFrameBuffering() const {
  player::AutoLock autolock(mutex_frame_queue_);
  if (IsFrameQueueEmpty()) {
    // Wait frame buffering
    senscord::osal::OSWaitCond(cond_frame_buffering_, mutex_frame_queue_);
  }
}

/**
 * @brief Thread that reads and queues frame files.
 */
void PlayerFrameFileManager::ReadFrameThread() {
  // file discriptor
  senscord::osal::OSFile* file = NULL;

  size_t read_position = start_position_;
  while (IsThreadStarted()) {
    if (IsFrameQueueMax()) {
      uint64_t sleep_time = GetReadSleepTime();
      SENSCORD_LOG_DEBUG("Queue Max: wait=%" PRIu64, sleep_time);
      senscord::osal::OSSleep(sleep_time);
      continue;
    }

    size_t index = read_position++;
    PlayFrame* frame = new PlayFrame();
    frame->index = static_cast<uint32_t>(index);
    senscord::Status status;

    // set frame information
    const uint64_t& sequence_number = play_frames_[index].sequence_number;
    const uint64_t& sent_time = play_frames_[index].sent_time;

    // set channel information
    RecordChannelData::const_iterator itr =
        play_frames_[index].channels.begin();
    for (; itr != play_frames_[index].channels.end(); ++itr) {
      const uint32_t& channel_id = itr->first;
      const uint64_t& captured_timestamp = itr->second.captured_timestamp;
      const senscord::RecordDataType& record_type = itr->second.record_type;
      const std::string& rawdata_type = itr->second.rawdata_type;

      // read channel property
      const BinaryPropertyList* property_list = NULL;
      property_list = GetChannelPropertyList(channel_id, sequence_number);
      if (property_list == NULL) {
        // In case of a read error, the channel is not output
        SENSCORD_LOG_DEBUG(
            "Failed to acquire the channel property : id=%" PRIu32, channel_id);
        continue;
      }
      frame->properties.insert(std::make_pair(channel_id, *property_list));

      // read and allocate channel rawdata
      senscord::Memory* memory = NULL;
      if (record_type == senscord::kRecordDataTypeRaw) {
        // raw format
        status = player::ReadRawFile(
            allocator_, target_path_, channel_id, sequence_number, &memory);
      } else {
        // composite raw format
        if (file == NULL) {
          size_t file_size = 0;
          // file open (Close after reading all channels)
          status = player::OpenFile(raw_index_path_, &file, &file_size);
          if (!status.ok()) {
            SENSCORD_LOG_DEBUG(
                "OpenFile error: ret=%s", status.ToString().c_str());
            continue;
          }
        }

        // Allocate raw data for the specified channel
        RawIndexDataWithOffset* raw_index =
            FindRawIndex(sequence_number, channel_id);
        status = AllocateCompositeRawData(
            allocator_, raw_index, &memory, file);
      }
      if (!status.ok()) {
        // In case of a read error, the channel is not output
        SENSCORD_LOG_DEBUG(
            "Failed to acquire the channel rawdata :"
            "[%" PRIu32 "ch] %s", channel_id, status.ToString().c_str());
        continue;
      }

      senscord::ChannelRawData channel = {};
      channel.channel_id = channel_id;
      channel.data_type = rawdata_type;
      channel.data_memory = memory;
      channel.data_size = memory->GetSize();
      channel.data_offset = 0;
      channel.captured_timestamp = captured_timestamp;
      frame->frame_info.channels.push_back(channel);
    }

    // Only for composite raw
    if (file != NULL) {
      senscord::osal::OSFclose(file);
      file = NULL;
    }

    {
      player::AutoLock autolock(mutex_frame_queue_);
      frame_queue_.push_back(std::make_pair(sent_time, frame));

      // Notify the completion of the buffering.
      senscord::osal::OSSignalCond(cond_frame_buffering_);
    }

    if (read_position >= play_frames_.size()) {
      read_position = 0;
    }
  }  // while (IsThreadStarted())

  // release the remaining frames.
  while (!frame_queue_.empty()) {
    PlayFrameQueue::iterator itr = frame_queue_.begin();

    // Release allocated rawdata memory
    for (size_t i = 0; i < itr->second->frame_info.channels.size(); ++i) {
      allocator_->Free(itr->second->frame_info.channels[i].data_memory);
    }

    // Release frame data
    delete itr->second;
    frame_queue_.erase(itr);
  }
}

/**
 * @brief Set the playback start position.
 * @param[in] (position) Playback start position to set.
 */
void PlayerFrameFileManager::SetPlayStartPosition(const uint32_t position) {
  start_position_ = position;
}

/**
 * @brief memory is allocated in this API and copy raw_index->rawdata to this
 * memory.
 * @param[in] (allocator) allocator
 * @param[in] (raw_index) raw_index->rawdata is data source
 * @param[out] memory is allocated in this API and copy raw_index->rawdata to
 * this memory
 * @param[in] (fp) The file pointer of raw_index.dat
 * @return Status object.
 */
senscord::Status PlayerFrameFileManager::AllocateCompositeRawData(
    senscord::MemoryAllocator* allocator,
    const RawIndexDataWithOffset* raw_index,
    senscord::Memory** memory,
    senscord::osal::OSFile* fp) {
  senscord::Status status;

  if (allocator == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "allocator is NULL");
  }
  if (raw_index == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "raw_index is NULL");
  }
  if (memory == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "memory is NULL");
  }

  if (raw_index->record_type != senscord::kRecordDataTypeCompositeRaw) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseInvalidArgument,
        "invalid record_type");
  }

  // seek and read
  uint8_t* read_buffer = new uint8_t[raw_index->size];
  status = player::ReadFile(
      fp, read_buffer, raw_index->size, raw_index->offset);
  if (!status.ok()) {
    delete[] read_buffer;
    return SENSCORD_STATUS_TRACE(status);
  }

  // deserialize
  senscord::ChannelRawDataForRawIndex record = {};
  senscord::serialize::Decoder decoder(
      read_buffer, raw_index->size);
  status = decoder.Pop(record);
  if (!status.ok()) {
    delete[] read_buffer;
    return SENSCORD_STATUS_TRACE(status);
  }
  delete[] read_buffer;

  // allocate raw data memory
  status = allocator->Allocate(record.rawdata.size(), memory);
  if (!status.ok()) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseResourceExhausted,
        "fail to allocate memory for CompositeRaw: size=%" PRIuS,
        record.rawdata.size());
  }

  senscord::osal::OSMemcpy(
      reinterpret_cast<void*>((*memory)->GetAddress()), (*memory)->GetSize(),
      &record.rawdata[0], record.rawdata.size());

  return senscord::Status::OK();
}

/**
 * @brief Set the wait time for when the queue size is maximum
 */
void PlayerFrameFileManager::SetReadSleepTime(
    uint32_t num, uint32_t denom) {
  double rate = static_cast<double>(num) / static_cast<double>(denom);
  uint64_t read_sleep_time =
    static_cast<uint64_t>((1ULL * 1000 * 1000 * 1000) / rate);
  read_sleep_time_ = read_sleep_time * kReadSleepCoefficient;
}

/**
 * @brief Get the wait time for when the queue size is maximum
 */
uint64_t PlayerFrameFileManager::GetReadSleepTime() const {
  return read_sleep_time_;
}
