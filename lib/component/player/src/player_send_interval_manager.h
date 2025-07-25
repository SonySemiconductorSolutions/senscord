/*
 * SPDX-FileCopyrightText: 2020-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_COMPONENT_PLAYER_SRC_PLAYER_SEND_INTERVAL_MANAGER_H_
#define LIB_COMPONENT_PLAYER_SRC_PLAYER_SEND_INTERVAL_MANAGER_H_

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <set>

#include "senscord/develop/component.h"
#include "senscord/develop/recorder_common.h"
#include "senscord/osal.h"
#include "senscord/osal_inttypes.h"
#include "senscord/property_types.h"
#include "senscord/noncopyable.h"
#include "./player_component_types.h"
#include "./player_component_util.h"

// pre-definition
class PlayerComponentPortData;  // user in IsFrameQueueEmpty

class PlayerSendIntervalManager : private senscord::util::Noncopyable {
 public:
  /**
   * @brief Constructor.
   */
  PlayerSendIntervalManager();

  /**
   * @brief Destructor.
   */
  ~PlayerSendIntervalManager();

  /**
   * @brief Setup send interval manager
   * @param[in] (port_id) The id of playback port.
   * @param[in] (sent_time_list) The time list of playback frames
   * @param[in] (port_instance) The instance of playback port
   * @return Status object.
   */
  senscord::Status SetupSendIntervalManager(
      int32_t port_id, const std::vector<uint64_t>& sent_time_list,
      PlayerComponentPortData* port_instance);

  /**
   * @brief Finalize send interval manager
   * @param[in] (port_id) The id of playback port.
   * @return Status object.
   */
  senscord::Status FinalizeSendIntervalManager(int32_t port_id);

  /**
   * @brief Start a thread
   * @return Status object
   */
  senscord::Status StartThreading(int32_t port_id);

  /**
   * @brief Stop a thread
   */
  void StopThreading(int32_t port_id);

  /**
   * @brief Set thread started flag.
   * @param[in] (port_id) The id of port
   * @param[in] (is_started) The flag of thread started
   */
  void SetThreadStarted(int32_t port_id, bool is_started);

  /**
   * @brief Get the count of management ports
   * @return The count of management ports
   */
  size_t GetSendManagePortCount();

  /**
   * @brief Get the count of sending ports
   * @return The count of sending ports
   */
  uint8_t GetSendStartedCount();

  /**
   * @brief Wait condition for send thread.
   * @param[in] (port_id) The id of port
   * @return Status object.
   */
  senscord::Status WaitInterval(int32_t port_id);

  /**
   * @brief Set repeat property of play mode.
   * @param[in] (prop) The property of player.
   */
  void SetRepeatMode(bool is_repeat);

  /**
   * @brief Set frame rate
   * @param[in] (port_id) The id of management port
   * @param[in] (num) The num of framerate
   * @param[in] (denom) The denom of framerate
   */
  void SetFrameRate(int32_t port_id, uint32_t num, uint32_t denom);

  /**
   * @brief Publish send interval thread
   */
  void SendIntervalThread();

  /**
   * @brief Set state of frame wait
   * @param[in] (port_id) The id of port
   * @param[in] (frame_wait) The state of frame wait
   */
  void SetFrameWait(int32_t port_id, bool frame_wait);

  /**
   * @brief Set the playback start position for asynchronous playback.
   * @param[in] (position) Playback start position to set.
   */
  void SetAsyncPlayStartPosition(size_t position);

 private:
  struct PortManageInfo {
    PlayerComponentPortData* port_instance;
    bool is_started;
    senscord::osal::OSMutex* mutex_send_wait;
    senscord::osal::OSCond* cond_send_wait;
    std::vector<uint64_t> sent_time_list;
    uint32_t num;
    uint32_t denom;
    uint32_t sent_count;
  };

  // key=port_id
  typedef std::map<int32_t, PortManageInfo> PortManageList;
  PortManageList send_manage_list_;
  senscord::osal::OSMutex* mutex_port_manage_;

  senscord::osal::OSThread* signal_thread_;
  bool is_thread_started_;
  mutable senscord::osal::OSMutex* mutex_thread_started_;

  bool is_repeat_;
  uint64_t sleep_time_;
  size_t async_start_position_;

  /**
   * @brief Check the state of the thread
   * @return true: started, false: not started
   */
  bool IsThreadStarted() const;

  /**
   * @brief Set sleep time of send interval.
   */
  void SetSleepTime(uint32_t num, uint32_t denom);

  /**
   * @brief Set sleep time of send interval for synchronized playback.
   */
  void SetSyncSleepTime();

  /**
   * @brief Get the sleep time for the send interval
   * @return The sleep time calculated from the frame rate
   */
  uint64_t GetSleepTime();

  /**
   * @brief Check if the queue size is the empty
   * @return true: empty, false: not empty
   */
  bool IsFrameQueueEmpty();

  /**
   * @brief Check if port is already started
   * @param[in] (port_id) The id of port
   * @param[in/out] (started_ports) The list of started port id
   * @param[in] (sent_time) The timestamp of sync position
   * @return true: Started, false: Not started
   * @note Use only synchronized playback
   */
  bool CheckPortStarted(
      int32_t port_id, std::set<int32_t>* started_ports, uint64_t sent_time);

  /**
   * @brief Broadcast condition for clock thread.
   * @param[in] (port_id) The id of port
   * @param[in] (frame_sending) If True, call for sending Frame
   */
  void SignalCond(int32_t port_id, bool frame_sending);

  /**
   * @brief Processing of signals to the send thread for asynchronous playback
   */
  void SignalProcessing();

  /**
   * @brief Processing of signals to the send thread for synchronized playback
   */
  void SynchronousSignalProcessing();

  /**
   * @brief Get the time of playback range
   * @param[out] (start_time) Start time of the playback frame
   * @param[out] (end_time) End time of the playback frame
   * @return Status object.
   */
  senscord::Status GetSyncPlayRange(uint64_t* start_time, uint64_t* end_time);

  // Key=sent time, Value=port id list
  typedef std::map<uint64_t, std::vector<int32_t> > PortIdTableBySendTime;

  /**
   * @brief Create port id table by send timestamp
   * @param[out] (port_table) Port num table by send timestamp
   * @note Use only synchronized playback
   */
  void CreatePortIdTable(PortIdTableBySendTime* port_table);

  /**
   * @brief Get send port list
   * @param[in] (position) Current position of playback.
   * @param[in] (range) Range of time to acquire.
   * @param[in] (port_table) Port id table by send timestamp.
   * @param[out] (send_ports) Port id table by current range.
   * @note Use only synchronized playback
   */
  void GetSendPortList(uint64_t position, uint64_t range,
      const PortIdTableBySendTime& port_table,
      PortIdTableBySendTime* send_ports);

  /**
   * @brief Get the correction time for SendFrame interval.
   * @param [in] (sleep_time) The sleep time of SendFrame interval.
   * @param [in/out] (prev_timestamp) The timestamp of last SendFrame time.
   * @param [in/out] (prev_correction) Last correction time.
   */
  uint64_t GetCorrectionTime(uint64_t sleep_time, uint64_t* prev_timestamp,
      uint64_t* prev_correction) const;
};

#endif  // LIB_COMPONENT_PLAYER_SRC_PLAYER_SEND_INTERVAL_MANAGER_H_
