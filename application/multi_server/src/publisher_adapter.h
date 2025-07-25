/*
 * SPDX-FileCopyrightText: 2023-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APPLICATION_MULTI_SERVER_SRC_PUBLISHER_ADAPTER_H_
#define APPLICATION_MULTI_SERVER_SRC_PUBLISHER_ADAPTER_H_

#include <string>
#include <vector>
#include <list>
#include <map>
#include "senscord/osal.h"
#include "senscord/messenger.h"
#include "senscord/connection_types.h"
#include "senscord/connection.h"
#include "resource_adapter.h"
#include "client_adapter.h"

namespace senscord {
namespace server {

class ClientAdapter;

/**
 * @brief The adapter class to access to the stream.
 */
class PublisherAdapter : public ResourceAdapter {
 public:
  /**
   * @brief Open publisher adapter.
   * @param[in] (msg) Request message.
   * @param[in] (core) Core object.
   * @return Status object.
   */
  Status Open(const MessageDataOpenPublisherRequest& msg, Core* core);

  /**
   * @brief Close publisher adapter.
   * @param[in] (core) Core object.
   * @return Status object.
   */
  virtual Status Close(Core* core);

  /**
   * @brief Start to access to the stream.
   * @return Status object.
   */
  virtual Status StartMonitoring();

  /**
   * @brief Stop to access to the stream.
   * @return Status object.
   */
  virtual Status StopMonitoring();

  /**
   * @brief The method to monitor new messages.
   */
  virtual void Monitoring(ResourceAdapter::MonitorType type);

  /**
   * @brief Push new message for this stream.
   * @param[in] (msg) New incoming message instance.
   */
  virtual void PushMessage(const Message* msg);

  /**
   * @brief Release the frame if arrived.
   */
  Status ReleaseFrame(const FrameInfo& frameinfo);

  /**
   * @brief Get the stream pointer.
   * @return The pointer of this stream.
   */
  Publisher* GetPublisher() const;

  /**
   * @brief Constructor.
   * @param[in] (stream) The pointer of stream.
   * @param[in] (client) The adapter to client.
   */
  explicit PublisherAdapter(ClientAdapter* client);

  /**
   * @brief Destructor.
   */
  ~PublisherAdapter();

 private:
  typedef std::list<const Message*> MessageList;

  /**
   * Information for ReleaseFrame.
   */
  struct ReleaseFrameInfo {
    Frame* frame;
    bool rawdata_accessed;
  };

  /**
   * @brief Pop the latest messgae. if not incoming, wait forever.
   * @return The latest message.
   */
  const Message* PopMessage(MessageList* messages);

  /**
   * @brief Do the calling function to stream each message types.
   * @param[in] (msg) The message for doing.
   */
  void DoMessage(const Message& msg);

  /**
   * @brief Release the frame to the stream.
   * @param[in] (msg) The message for doing.
   * @return The status of send the reply.
   */
  Status PublishFrames(const Message& msg);

  /**
   * @brief Return whether the FrameProperty has been updated.
   * @param[in] (src) The frame message from the client.
   * @return whether the FrameProperty has been updated.
   */
  bool IsUpdatedFrameProperty(const MessageDataFrameLocalMemory& src) const;

  /**
   * @brief Update properties by the frame from the server.
   * @param[in] (port) The component port.
   * @param[in] (src) The frame message from the server.
   * @return Status object.
   */
  Status UpdateFrameProperties(
      const senscord::MessageDataFrameLocalMemory& src);

  /**
   * @brief Whether to reply to SendFrame message.
   * @param[in] (frame) The frame message from the server.
   * @return The need to reply.
   */
  bool IsReplyToSendFrame(const MessageDataFrameLocalMemory& frame) const;

  /**
   * @brief Create the frame info for SendFrame.
   * @param[out] (dest) The destination of FrameInfo.
   * @param[in] (src) The frame message from the client.
   * @return Status object.
   */
  Status CreateFrameInfo(
      FrameInfo* dest,
      const MessageDataFrameLocalMemory& src);

  /**
   * @brief Memory mapping the rawdata.
   * @param[in] (key) The memory allocator key.
   * @param[in] (serialized) The serialized memory data.
   * @param[in] (memory) The mapped memory.
   * @return Status object.
   */
  Status MemoryMapping(
      const std::string& key, const std::vector<uint8_t>& serialized,
      MemoryContained* memory);

  Publisher* publisher_;
  ClientAdapter* client_;

  MessageList messages_;

  osal::OSThread* thread_;

  osal::OSMutex* messaging_mutex_;
  osal::OSCond* messaging_cond_;
  bool end_flag_;

  typedef std::map<std::string, MemoryAllocator*> Allocators;
  Allocators allocators_;
};

}  // namespace server
}  // namespace senscord

#endif  // APPLICATION_MULTI_SERVER_SRC_PUBLISHER_ADAPTER_H_
