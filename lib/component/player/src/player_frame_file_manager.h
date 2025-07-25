/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PLAYER_SRC_PLAYER_FRAME_FILE_MANAGER_H_
#define LIB_COMPONENT_PLAYER_SRC_PLAYER_FRAME_FILE_MANAGER_H_

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <utility>  // std::pair

#include "./player_common.h"
#include "./player_component_types.h"
#include "senscord/develop/component.h"
#include "senscord/develop/recorder_common.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/property_types.h"

class PlayerFrameFileManager {
 public:
  /**
   * @brief Constructor.
   */
  explicit PlayerFrameFileManager(senscord::MemoryAllocator* allocator);

  /**
   * @brief Destructor.
   */
  ~PlayerFrameFileManager();

  /**
   * @brief Setup frame file manager
   * @param[in] (play_property) The play property
   * @param[in] (channels) The channel information of info.xml
   * @param[in] (buffer_size) The size of raw index buffer.
   * @return Status object.
   */
  senscord::Status SetupFrameFileManager(
      const std::string& play_property,
      const InfoXmlChannelList& channels,
      const size_t buffer_size);

  /**
   * @brief Adjust frame queue by timestamp. (Discard old frame from the queue)
   * @param[in] (sent_time) The timestamp of sync position.
   * @note Use only synchronized playback.
   */
  void AdjustFrameQueueByTimestamp(uint64_t sent_time);

  /**
   * @brief Get the frame from queue
   * @param[out] (frame) frame data
   * @return status object
   */
  senscord::Status GetFrame(PlayFrame** frame);

  /**
   * @brief Clear raw index.
   */
  void ClearRawIndex();

  /**
   * @brief Clear PlayerComponentChannelData.
   */
  void ClearChannel();

  /**
   * @brief Set playback range.
   * @param[in] (offset) Start offset.
   * @param[in] (count) Number from start offset.
   * @return Status object.
   */
  senscord::Status SetPlaybackRange(uint32_t offset, uint32_t count);

  /**
   * @brief Get the count of playback frames
   */
  uint32_t GetPlayCount();

  /**
   * @brief Get the count of total frames.
   */
  uint32_t GetTotalFrameCount();

  /**
   * @brief Get the sent time list of playback frames
   * @return The list of sent time
   */
  std::vector<uint64_t> GetSentTimeList();

  /**
   * @brief Get ChannelInfoProperty.
   * @param[out] (prop) The property of channel info.
   */
  void GetChannelInfo(senscord::ChannelInfoProperty* prop);

  /**
   * @brief Set thread started flag.
   * @param[in] (is_started) The flag of thread started
   */
  void SetThreadStarted(bool is_started);

  /**
   * @brief Start a thread
   * @return Status object
   */
  senscord::Status StartThreading();

  /**
   * @brief Stop a thread
   */
  void StopThreading();

  /**
   * @brief Check if the queue size is the empty
   * @return true: empty, false: not empty
   */
  bool IsFrameQueueEmpty() const;

  /**
   * @brief Set the wait time for when the queue size is maximum
   */
  void SetReadSleepTime(uint32_t num, uint32_t denom);

  /**
   * @brief Thread that reads and queues frame files.
   */
  void ReadFrameThread();

  /**
   * @brief Wait frame buffering.
   */
  void WaitFrameBuffering() const;

  /**
   * @brief Set the playback start position.
   * @param[in] (position) Playback start position to set.
   */
  void SetPlayStartPosition(const uint32_t position);

 private:
  /**
   * @brief Raw data storage of one channel for raw_index.dat file.
   */
  struct RawIndexDataWithOffset {
    uint64_t sequence_number;         /**< Sequence number */
    uint32_t channel_id;              /**< Channel ID */
    uint64_t captured_timestamp;      /**< Captured timestamp */
    uint64_t sent_time;               /**< Sent time */
    senscord::RecordDataType record_type;       /**< Record type */
    size_t offset;   /**< Offset of RawIndex data in the file */
    size_t size;     /**< Size of RawIndex data size in the file */
  };

  std::string target_path_;
  uint64_t read_sleep_time_;
  senscord::MemoryAllocator* allocator_;
  senscord::osal::OSThread* read_thread_;
  bool is_started_;
  mutable senscord::osal::OSMutex* mutex_started_;

  // frames
  typedef std::vector<std::pair<uint64_t, PlayFrame*> > PlayFrameQueue;
  mutable senscord::osal::OSMutex* mutex_frame_queue_;
  PlayFrameQueue frame_queue_;
  senscord::osal::OSCond* cond_frame_buffering_;

  typedef std::vector<RawIndexDataWithOffset> RawIndexList;
  RawIndexList raw_index_;
  std::vector<RecordFrameData> total_frames_;
  std::vector<RecordFrameData> play_frames_;
  std::map<uint32_t, PlayerComponentChannelData*> channel_list_;

  // raw index path
  std::string raw_index_path_;

  // start seek position
  uint32_t start_position_;

  /**
   * @brief Read raw index from record file.
   * @param[in] (target_path) Target directory for read to raw index.
   * @param[in] (channels) The channel information of info.xml
   * @param[in] (buffer_size) The size of raw index buffer.
   * @return Status object.
   */
  senscord::Status ReadRawIndex(
      const std::string& target_path, const InfoXmlChannelList& channels,
      const size_t buffer_size);

  /**
   * @brief Deserialize the data and apply it to the raw index list.
   * @param[in] (read_buffer) Data buffer read from raw_index.dat.
   * @param[in] (read_size) The size of file read.
   * @param[out] (raw_index_list) List of raw index data.
   * @param[in/out] (deserialized_size) Deserialized data size of the raw_index.dat.
   * @return Status object.
   */
  senscord::Status DeserializeRawIndexData(
      const void* read_buffer, size_t read_size,
      RawIndexList* raw_index_list, size_t* deserialized_size);

  /**
   * @brief Find for raw index by sequence number and channel id
   * @param[in] (sequence_number) The sequence number
   * @param[in] (channel_id) The channel id
   * @return The pointer of raw index data
   */
  RawIndexDataWithOffset* FindRawIndex(
      uint64_t sequence_number, uint32_t channel_id);

  /**
   * @brief Read ChannelPropertyFile.
   * @param[in] (target_path) Target directory for read to channel data.
   * @param[in] (channels) The channel information of info.xml
   * @return Status object.
   */
  senscord::Status ReadChannelProperty(
      const std::string& target_path, const InfoXmlChannelList& channels);

  /**
   * @brief read channel property file "channel_0xHHHHHHHH/properties.dat"
   * and create PlayerComponentChanData::PlayerComponentPropertyListBySeqNo
   * property_list
   * @param[in] (target_path) Target directory for read to channel data.
   * @param[in] (channel_id) channel_id
   * @param[in] (p_list) data to be created
   * @return Status object.
   */
  senscord::Status ReadChannelPropertyFile(
      const std::string& target_path, uint32_t channel_id,
      PlayerComponentPropertyListBySeqNo* p_list);

  /**
   * @brief find channel property by sequence_number
   * @param[in] (channel_id) channel_id
   * @param[in] (sequence_number) sequence_number
   * @return The list of binary properties
   */
  const BinaryPropertyList* GetChannelPropertyList(
      uint32_t channel_id, uint64_t sequence_number);

  /**
   * @brief Find PlayerComponentChannelData.
   * @param[in] (channel_id) Target channel ID.
   * @return Channel data.
   */
  PlayerComponentChannelData* GetChannelData(uint32_t channel_id);

  /**
   * @brief memory is allocated in this API and copy raw_index->rawdata to this
   * memory.
   * @param[in] (allocator) allocator
   * @param[in] (raw_index) raw_index->rawdata is data source
   * @param[out] (memory) allocated in this API and copy raw_index->rawdata to
   * this memory
   * @param[in] (fp) The file pointer of raw_index.dat
   * @return Status object.
   */
  senscord::Status AllocateCompositeRawData(
      senscord::MemoryAllocator* allocator,
      const RawIndexDataWithOffset* raw_index,
      senscord::Memory** memory,
      senscord::osal::OSFile* fp);

  /**
   * @brief Check the state of the thread
   * @return true: started, false: not started
   */
  bool IsThreadStarted() const;

  /**
   * @brief Check if the queue size is the maximum
   * @return true: max, false: not max
   */
  bool IsFrameQueueMax() const;

  /**
   * @brief Thread that reads and queues frame files.
   */
  uint64_t GetReadSleepTime() const;
};

#endif  // LIB_COMPONENT_PLAYER_SRC_PLAYER_FRAME_FILE_MANAGER_H_
