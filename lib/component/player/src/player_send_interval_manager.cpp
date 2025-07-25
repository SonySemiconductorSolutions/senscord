/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "player_send_interval_manager.h"

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <string>
#include <utility>  // std::make_pair
#include <vector>
#include <set>

#include "senscord/logger.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "./player_component_util.h"
#include "./player_property_accessor.h"
#include "./player_component_port_data.h"
#include "./player_autolock.h"

namespace {
const char* kModuleName = "player_send_interval_manager";
const uint64_t kWaitIntervalTimeout = 1ULL * 1000 * 1000 * 1000;
}  // namespace

/**
 * @brief Worker thread entry
 */
static senscord::osal::OSThreadResult WorkerThreadEntry(void* arg) {
  PlayerSendIntervalManager* adapter =
      reinterpret_cast<PlayerSendIntervalManager*>(arg);
  if (adapter != NULL) {
    adapter->SendIntervalThread();
  }
  return 0;
}

/**
 * @brief Constructor.
 */
PlayerSendIntervalManager::PlayerSendIntervalManager()
    : send_manage_list_(),
      mutex_port_manage_(NULL),
      signal_thread_(NULL),
      is_thread_started_(false),
      mutex_thread_started_(NULL),
      is_repeat_(false),
      sleep_time_(0),
      async_start_position_(0) {
  senscord::osal::OSCreateMutex(&mutex_port_manage_);
  senscord::osal::OSCreateMutex(&mutex_thread_started_);
}

/**
 * @brief Destructor.
 */
PlayerSendIntervalManager::~PlayerSendIntervalManager() {
  senscord::osal::OSDestroyMutex(mutex_port_manage_);
  mutex_port_manage_ = NULL;
  senscord::osal::OSDestroyMutex(mutex_thread_started_);
  mutex_thread_started_ = NULL;
}

/**
 * @brief Setup send interval manager
 * @param[in] (port_id) The id of playback port.
 * @param[in] (sent_time_list) The time list of playback frames
 * @param[in] (port_instance) The instance of playback port
 * @return Status object.
 */
senscord::Status PlayerSendIntervalManager::SetupSendIntervalManager(
    int32_t port_id, const std::vector<uint64_t>& sent_time_list,
    PlayerComponentPortData* port_instance) {
  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::iterator found = send_manage_list_.find(port_id);
  if (found == send_manage_list_.end()) {
    // Set port manage data
    PortManageInfo port_info = {};
    port_info.port_instance = port_instance;
    port_info.is_started = false;
    port_info.sent_time_list = sent_time_list;
    port_info.sent_count = 0;
    senscord::osal::OSCreateMutex(&port_info.mutex_send_wait);
    senscord::osal::OSCreateCond(&port_info.cond_send_wait);

    // Append send manage port
    send_manage_list_.insert(std::make_pair(port_id, port_info));
  } else {
    // Update time list
    found->second.sent_time_list = sent_time_list;
  }

  return senscord::Status::OK();
}

/**
 * @brief Finalize send interval manager
 * @param[in] (port_id) The id of playback port.
 * @return Status object.
 */
senscord::Status PlayerSendIntervalManager::FinalizeSendIntervalManager(
    int32_t port_id) {
  // Erase send manage port
  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::iterator found = send_manage_list_.find(port_id);
  if (found != send_manage_list_.end()) {
    PortManageInfo& port_info = found->second;
    senscord::osal::OSDestroyMutex(port_info.mutex_send_wait);
    senscord::osal::OSDestroyCond(port_info.cond_send_wait);
    send_manage_list_.erase(found);
  }

  return senscord::Status::OK();
}

/**
 * @brief Set repeat property of play mode.
 * @param[in] (prop) The property of player.
 */
void PlayerSendIntervalManager::SetRepeatMode(
    bool is_repeat) {
  is_repeat_ = is_repeat;
}

/**
 * @brief Set frame rate
 * @param[in] (port_id) The id of management port
 * @param[in] (num) The num of framerate
 * @param[in] (denom) The denom of framerate
 */
void PlayerSendIntervalManager::SetFrameRate(
    int32_t port_id, uint32_t num, uint32_t denom) {
  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::iterator found = send_manage_list_.find(port_id);
  if (found != send_manage_list_.end()) {
    found->second.num = num;
    found->second.denom = denom;
  }
  SetSleepTime(num, denom);
}

/**
 * @brief Set the playback start position for asynchronous playback.
 * @param[in] (position) Playback start position to set.
 */
void PlayerSendIntervalManager::SetAsyncPlayStartPosition(
    const size_t position) {
  player::AutoLock autolock(mutex_port_manage_);
  async_start_position_ = position;
}

/**
 * @brief Get the count of management ports
 * @return The count of management ports
 */
size_t PlayerSendIntervalManager::GetSendManagePortCount() {
  player::AutoLock autolock(mutex_port_manage_);
  return send_manage_list_.size();
}

/**
 * @brief Get the count of sending ports
 * @return The count of sending ports
 */
uint8_t PlayerSendIntervalManager::GetSendStartedCount() {
  player::AutoLock autolock(mutex_port_manage_);

  uint8_t count = 0;
  PortManageList::const_iterator itr = send_manage_list_.begin();
  PortManageList::const_iterator end = send_manage_list_.end();
  for (; itr != end; ++itr) {
    if (itr->second.is_started) {
      ++count;
    }
  }

  return count;
}

/**
 * @brief Check if port is already started
 * @param[in] (port_id) The id of port
 * @param[in/out] (started_ports) The list of started port id
 * @param[in] (sent_time) The timestamp of sync position
 * @return true: Started, false: Not started
 * @note Use only synchronized playback
 */
bool PlayerSendIntervalManager::CheckPortStarted(
    int32_t port_id, std::set<int32_t>* started_ports, uint64_t sent_time) {
  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::iterator found = send_manage_list_.find(port_id);
  if (found == send_manage_list_.end()) {
    // Unmanaged port id (Streams that are not open)
    return false;
  }

  // Set the start state
  bool is_started = found->second.is_started;
  if (!is_started) {
    // erase port id from started list
    started_ports->erase(port_id);
    return is_started;
  }

  // if newly started.
  if (started_ports->insert(port_id).second) {
    // Adjust the frame queue to fit the sync position.
    const PortManageInfo& port_info = found->second;
    port_info.port_instance->AdjustFrameQueueByTimestamp(sent_time);
  }

  return is_started;
}

/**
 * @brief Set thread started flag.
 * @param[in] (port_id) The id of port
 * @param[in] (is_started) The flag of thread started
 */
void PlayerSendIntervalManager::SetThreadStarted(
    int32_t port_id, bool is_started) {
  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::iterator found = send_manage_list_.find(port_id);
  if (found != send_manage_list_.end()) {
    found->second.is_started = is_started;
  }
}

/**
 * @brief Start a thread
 * @return Status object
 */
senscord::Status PlayerSendIntervalManager::StartThreading(int32_t port_id) {
  // Start a thread for the first time only
  if (GetSendStartedCount() == 1) {
    player::AutoLock autolock(mutex_thread_started_);
    // set state
    is_thread_started_ = true;

    int32_t ret = senscord::osal::OSCreateThread(
        &signal_thread_, WorkerThreadEntry, this, NULL);
    if (ret != 0) {
      return SENSCORD_STATUS_FAIL(kModuleName,
          senscord::Status::kCauseAborted,
          "failed to OSCreateThread: %" PRIx32, ret);
    }
  }
  return senscord::Status::OK();
}

/**
 * @brief Stop a thread
 */
void PlayerSendIntervalManager::StopThreading(int32_t port_id) {
  // Join a thread for the last time only
  if (GetSendStartedCount() == 0) {
    {
      player::AutoLock autolock(mutex_thread_started_);
      // set state
      is_thread_started_ = false;
    }
    senscord::osal::OSJoinThread(signal_thread_, NULL);
  }
  // Cond signal for stop of send thread
  SignalCond(port_id, false);
}

/**
 * @brief Check the state of the thread
 * @return true: started, false: not started
 */
bool PlayerSendIntervalManager::IsThreadStarted() const {
  player::AutoLock autolock(mutex_thread_started_);
  return is_thread_started_;
}

/**
 * @brief Signal condition for interval thread.
 * @param[in] (port_id) The id of port
 * @param[in] (frame_sending) If True, call for sending Frame
 */
void PlayerSendIntervalManager::SignalCond(
    int32_t port_id, bool frame_sending) {
  PortManageList::iterator found = send_manage_list_.find(port_id);
  PortManageInfo& info = found->second;
  player::AutoLock autolock(info.mutex_send_wait);
  if (frame_sending) {
    ++info.sent_count;
  }
  senscord::osal::OSSignalCond(info.cond_send_wait);
}

/**
 * @brief Wait condition for send thread.
 * @param[in] (port_id) The id of port
 * @return Status object.
 */
senscord::Status PlayerSendIntervalManager::WaitInterval(int32_t port_id) {
  int32_t ret = 0;
  PortManageList::iterator found = send_manage_list_.find(port_id);
  PortManageInfo& info = found->second;

  if (info.sent_count == 0) {
    ret = senscord::osal::OSRelativeTimedWaitCond(
        info.cond_send_wait, info.mutex_send_wait, kWaitIntervalTimeout);
  }

  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kModuleName, senscord::Status::kCauseAborted,
        "failed to wait interval: ret=%" PRIx32, ret);
  }
  if (info.sent_count > 0) {
    --info.sent_count;
  }
  return senscord::Status::OK();
}

/**
 * @brief Set state of frame wait
 * @param[in] (port_id) The id of port
 * @param[in] (frame_wait) The state of frame wait
 */
void PlayerSendIntervalManager::SetFrameWait(int32_t port_id, bool frame_wait) {
  PortManageList::iterator found = send_manage_list_.find(port_id);
  PortManageInfo& info = found->second;
  if (frame_wait) {
    senscord::osal::OSLockMutex(info.mutex_send_wait);
    info.sent_count = 0;
  } else {
    senscord::osal::OSUnlockMutex(info.mutex_send_wait);
  }
}

/**
 * @brief Publish send manage thread
 */
void PlayerSendIntervalManager::SendIntervalThread() {
  size_t port_count = GetSendManagePortCount();
  SENSCORD_LOG_DEBUG("SignalThread Start: port_count=%" PRIuS, port_count);
  if (port_count == 1) {
    SignalProcessing();
  } else {
    SynchronousSignalProcessing();
  }
  SENSCORD_LOG_DEBUG("SignalThread Stop");
}

/**
 * @brief Processing of signals to the send thread for asynchronous playback
 */
void PlayerSendIntervalManager::SignalProcessing() {
  int32_t port_id = 0;
  size_t frame_count = 0;
  size_t current_position = 0;
  {
    player::AutoLock autolock(mutex_port_manage_);
    PortManageList::const_iterator itr = send_manage_list_.begin();
    // set sleep time
    SetSleepTime(itr->second.num, itr->second.denom);
    // set port id
    port_id = itr->first;
    // set frame count
    frame_count = itr->second.sent_time_list.size();
    // set current position
    current_position = async_start_position_;
  }

  // signal processing
  uint64_t prev_timestamp = 0;
  uint64_t prev_correction = 0;

  while (IsThreadStarted()) {
    uint64_t sleep_time = GetSleepTime();
    if (IsFrameQueueEmpty()) {
      senscord::osal::OSSleep(sleep_time);
      continue;
    }

    if (current_position >= frame_count) {
      if (is_repeat_) {
        current_position = 0;
      } else {
        senscord::osal::OSSleep(sleep_time);
        continue;
      }
    }

    // Cond signal to send thread.
    SignalCond(port_id, true);

    // Seek play position
    ++current_position;

    uint64_t correction =
        GetCorrectionTime(sleep_time, &prev_timestamp, &prev_correction);
    senscord::osal::OSSleep(sleep_time - correction);
  }  // while (IsThreadStarted())
}

/**
 * @brief Processing of signals to the send thread for synchronized playback
 */
void PlayerSendIntervalManager::SynchronousSignalProcessing() {
  uint64_t start_time = 0;
  uint64_t end_time = 0;
  senscord::Status status = GetSyncPlayRange(&start_time, &end_time);
  if (!status.ok()) {
    SENSCORD_LOG_ERROR("%s", status.ToString().c_str());
    return;
  }
  uint64_t sync_position = start_time;

  // set sync sleep time
  SetSyncSleepTime();
  uint64_t sleep_time = GetSleepTime();

  std::set<int32_t> started_ports;

  uint64_t prev_timestamp = 0;
  uint64_t prev_correction = 0;
  // Create port num table by send timestamp
  PortIdTableBySendTime port_table;
  CreatePortIdTable(&port_table);
  while (IsThreadStarted()) {
    if (IsFrameQueueEmpty()) {
      senscord::osal::OSSleep(sleep_time);
      continue;
    }
    if (sync_position > end_time) {
      if (is_repeat_) {
        // Reset sync play position
        sync_position = start_time;
        SENSCORD_LOG_DEBUG("Reset play position");
      } else {
        senscord::osal::OSSleep(sleep_time);
        continue;
      }
    }

    // Get send port list
    PortIdTableBySendTime send_ports;
    GetSendPortList(sync_position, sleep_time, port_table, &send_ports);

    PortIdTableBySendTime::const_iterator itr = send_ports.begin();
    PortIdTableBySendTime::const_iterator end = send_ports.end();
    for (; itr != end; ++itr) {
      const std::vector<int32_t>& port_ids = itr->second;
      for (size_t i = 0; i < port_ids.size(); ++i) {
        if (!CheckPortStarted(port_ids[i], &started_ports, sync_position)) {
          // not port started
          continue;
        }

        // Cond signal to send thread.
        SignalCond(port_ids[i], true);
      }
    }
    // Seek play position
    sync_position += sleep_time;

    uint64_t correction =
        GetCorrectionTime(sleep_time, &prev_timestamp, &prev_correction);
    senscord::osal::OSSleep(sleep_time - correction);
  }  // while (IsThreadStarted())
}

/**
 * @brief Check if the queue size is the empty
 * @return true: empty, false: not empty
 */
bool PlayerSendIntervalManager::IsFrameQueueEmpty() {
  player::AutoLock autolock(mutex_port_manage_);
  bool is_empty = false;
  PortManageList::const_iterator itr = send_manage_list_.begin();
  PortManageList::const_iterator end = send_manage_list_.end();
  for (; itr != end; ++itr) {
    const PortManageInfo& info = itr->second;
    if (!info.is_started) {
      continue;
    }
    if (info.port_instance->IsFrameQueueEmpty()) {
      is_empty = true;
    }
  }
  return is_empty;
}

/**
 * @brief Set sleep time.
 */
void PlayerSendIntervalManager::SetSleepTime(
    uint32_t num, uint32_t denom) {
  sleep_time_ = (1ULL * 1000 * 1000 * 1000) * denom / num;
}

/**
 * @brief Set sleep time of send interval for synchronized playback.
 */
void PlayerSendIntervalManager::SetSyncSleepTime() {
  uint64_t sleep_time = 0;

  PortManageList::const_iterator itr = send_manage_list_.begin();
  PortManageList::const_iterator end = send_manage_list_.end();
  for (; itr != end; ++itr) {
    const PortManageInfo& info = itr->second;
    uint64_t time = (1ULL * 1000 * 1000 * 1000) * info.denom / info.num;
    if ((sleep_time == 0) || (sleep_time > time)) {
      sleep_time = time;
    }
  }

  sleep_time_ = sleep_time;
}

/**
 * @brief Get sleep time.
 * @return The sleep time calculated from the frame rate
 */
uint64_t PlayerSendIntervalManager::GetSleepTime() {
  return sleep_time_;
}

/**
 * @brief Get the time of playback range
 * @param[out] (start_time) Start time of the playback frame
 * @param[out] (end_time) End time of the playback frame
 * @return Status object.
 */
senscord::Status PlayerSendIntervalManager::GetSyncPlayRange(
    uint64_t* start_time, uint64_t* end_time) {
  if (start_time == NULL || end_time == NULL) {
    return SENSCORD_STATUS_FAIL(
        kModuleName, senscord::Status::kCauseAborted,
        "start_time or end_time is null");
  }

  player::AutoLock autolock(mutex_port_manage_);
  PortManageList::const_iterator itr = send_manage_list_.begin();
  PortManageList::const_iterator end = send_manage_list_.end();
  for (; itr != end; ++itr) {
    uint64_t front = itr->second.sent_time_list.front();
    uint64_t back = itr->second.sent_time_list.back();
    if (itr == send_manage_list_.begin()) {
      *start_time = front;
      *end_time = back;
      continue;
    }
    if (*start_time > front) {
      *start_time = front;
    }
    if (*end_time < back) {
      *end_time = back;
    }
  }

  return senscord::Status::OK();
}

/**
 * @brief Create port num table by send timestamp
 * @param[out] (port_table) Port num table by send timestamp
 * @note Use only synchronized playback
 */
void PlayerSendIntervalManager::CreatePortIdTable(
    PortIdTableBySendTime* port_table) {
  player::AutoLock autolock(mutex_port_manage_);
  // PortManageList: key=port_id, value=PortManageInfo
  PortManageList::iterator list_itr = send_manage_list_.begin();
  PortManageList::iterator list_end = send_manage_list_.end();
  for (; list_itr != list_end; ++list_itr) {
    int32_t port_id = list_itr->first;
    PortManageInfo& info = list_itr->second;
    for (std::vector<uint64_t>::const_iterator
        itr = info.sent_time_list.begin(), end = info.sent_time_list.end();
        itr != end; ++itr) {
      // Append port id (key=timestamp)
      (*port_table)[*itr].push_back(port_id);
    }
  }
}

/**
 * @brief GetSendPortList
 * @param[in] (position) Current position of playback.
 * @param[in] (offset) Range of time to acquire.
 * @param[in] (port_table) Port id table by send timestamp
 * @param[out] (send_ports) Port id table by send timestamp
 * @return Send port list by send timestamp
 * @note Use only synchronized playback
 */
void PlayerSendIntervalManager::GetSendPortList(
    uint64_t position, uint64_t offset,
    const PortIdTableBySendTime& port_table,
    PortIdTableBySendTime* send_ports) {
  uint64_t range = position + offset;

  PortIdTableBySendTime::const_iterator itr = port_table.begin();
  PortIdTableBySendTime::const_iterator end = port_table.end();
  for (; itr != end; ++itr) {
    // Check for out-of-range times
    if (itr->first < position) {
      continue;
    } else if (itr->first >= range) {
      break;
    }

    send_ports->insert(*itr);
  }
}

/**
 * @brief Get the correction time for SendFrame interval.
 * @param [in] (sleep_time) The sleep time of SendFrame interval.
 * @param [in/out] (prev_timestamp) The timestamp of last SendFrame time.
 * @param [in/out] (prev_correction) Last correction time.
 */
uint64_t PlayerSendIntervalManager::GetCorrectionTime(
    uint64_t sleep_time, uint64_t* prev_timestamp,
    uint64_t* prev_correction) const {
  // First time only to get time.
  if (*prev_timestamp == 0) {
    senscord::osal::OSGetTime(prev_timestamp);
    return 0;
  }

  uint64_t current_timestamp = 0;
  senscord::osal::OSGetTime(&current_timestamp);

  // Calculate the elapsed time since the last SendFrame.
  uint64_t diff = current_timestamp - *prev_timestamp;
  diff += *prev_correction;
  *prev_timestamp = current_timestamp;

  // Calculate the time to compensate.
  uint64_t correction_time = 0;
  if (diff > sleep_time) {
    correction_time = diff - sleep_time;
  }

  // If the correction time is greater than the sleep time,
  // no correction is made.
  if (sleep_time < correction_time) {
    correction_time = 0;
  }
  *prev_correction = correction_time;

  return correction_time;
}
