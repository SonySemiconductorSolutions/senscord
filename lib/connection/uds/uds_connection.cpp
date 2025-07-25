/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senscord/connection/uds_connection.h"
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include "senscord/osal_inttypes.h"
#include "senscord/osal.h"
#include "senscord/logger.h"
#include "senscord/serialize.h"
#include "senscord/memory_allocator.h"
#include "senscord/develop/socket_message_buffer.h"
#include "senscord/develop/connection_utils.h"

// export register function.
SENSCORD_REGISTER_CONNECTION(senscord::UdsConnection)

namespace {

const char* kArgumentBufferChunkSize = "buffer_chunk_size";
const char* kArgumentBufferWriteSizeThreshold = "buffer_write_size_threshold";
const char* kArgumentReceiveTimeout = "receive_timeout_msec";

/**
 * @brief Gets the uint32 type argument.
 */
uint32_t GetArgumentUint32(
    const std::map<std::string, std::string>& arguments,
    const std::string& key, uint32_t default_value) {
  uint32_t result = default_value;
  std::map<std::string, std::string>::const_iterator pos = arguments.find(key);
  if (pos != arguments.end()) {
    uint64_t num = 0;
    char* endptr = NULL;
    int32_t ret = senscord::osal::OSStrtoull(
        pos->second.c_str(), &endptr, senscord::osal::kOSRadixAuto, &num);
    if (ret == 0 && endptr != NULL && *endptr == '\0') {
      num = std::min(num, static_cast<uint64_t>(0xffffffff));
      result = static_cast<uint32_t>(num);
    }
  }
  return result;
}

/**
 * @brief Get the local address.
 *
 * If the first character is '@', convert it to abstract namespace.
 *
 * @param[in] (address) String of local path.
 * @param[out] (addr_un) Address for unix domain socket.
 * @param[out] (addr_size) Size of address.
 * @return Status object.
 */
senscord::Status GetLocalAddress(
    const std::string& address, sockaddr_un* addr_un, socklen_t* addr_size) {
  if (address.empty()) {
    return SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument, "address is empty");
  }
  // The maximum length of sun_path includes the termination code.
  if (address.size() > (sizeof(addr_un->sun_path) - 1)) {
    return SENSCORD_STATUS_FAIL(senscord::kStatusBlockCore,
        senscord::Status::kCauseInvalidArgument,
        "address is too long. (input=%" PRIuS ", max=%" PRIuS ")",
        address.size(), (sizeof(addr_un->sun_path) - 1));
  }

  std::string address_tmp = address;
#ifdef __APPLE__
  if (address[0] == '@') {
    address_tmp.erase(0, 1);  // remove '@'
  }
#endif  // __APPLE__

  addr_un->sun_family = AF_UNIX;
  senscord::osal::OSMemcpy(addr_un->sun_path, sizeof(addr_un->sun_path),
                           address_tmp.c_str(), address_tmp.size());

  // If the first character is '@', convert it to abstract namespace.
  if (addr_un->sun_path[0] == '@') {
    addr_un->sun_path[0] = '\0';
    *addr_size = static_cast<socklen_t>(
        sizeof(addr_un->sun_family) + address_tmp.size());
  } else {
    *addr_size = sizeof(sockaddr_un);
  }

  return senscord::Status::OK();
}

/**
 * @brief Convert nanoseconds to timeval.
 */
timeval ToTimeval(uint64_t nano_seconds) {
  // round-up (+999 nanoseconds).
  lldiv_t val = lldiv(nano_seconds + 999, 1000LL * 1000 * 1000);

  timeval tval = {};
  tval.tv_sec = val.quot;
  tval.tv_usec = static_cast<suseconds_t>(val.rem / 1000);

  return tval;
}

/**
 * @brief Convert socket fd to OSSocket.
 */
senscord::osal::OSSocket* GetOSSocket(int32_t socket_fd) {
  intptr_t ptr = static_cast<intptr_t>(socket_fd);
  senscord::osal::OSSocket* socket =
      reinterpret_cast<senscord::osal::OSSocket*>(ptr);
  return socket;
}

}  // namespace

namespace senscord {

// backlog size for listen.
static const int32_t kBacklogSize = 3;

/**
 * @brief Open the connection.
 * @param[in] (arguments) connection arguments.
 * @return Status object.
 */
Status UdsConnection::Open(
    const std::map<std::string, std::string>& arguments) {
  arguments_ = arguments;
  buffer_chunk_size_ = GetArgumentUint32(
      arguments, kArgumentBufferChunkSize, 0);
  buffer_write_threshold_ = GetArgumentUint32(
      arguments, kArgumentBufferWriteSizeThreshold, 0);
  receive_timeout_msec_ = GetArgumentUint32(
      arguments, kArgumentReceiveTimeout, 0);
  return Open();
}

/**
 * @brief Open the connection.
 * @return Status object.
 */
Status UdsConnection::Open() {
  if (socket_ != -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "already opened");
  }

  int32_t socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to create socket: errno=%d",
        errno);
  }

  socket_ = socket_fd;

  return Status::OK();
}

/**
 * @brief Close the connection.
 * @return Status object.
 */
Status UdsConnection::Close() {
  if (socket_ != -1) {
    // force shutdown
    shutdown(socket_, SHUT_RDWR);

    // close
    int32_t ret = close(socket_);
    if (ret < 0) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation, "failed to destroy socket: errno=%d",
          errno);
    }
    socket_ = -1;

    // Remove the socket file generated by bind().
    // Because it is a device file, call unlink() directly, not osal::OSRemove.
    if (!socket_path_.empty()) {
      ret = unlink(socket_path_.c_str());
      if (ret < 0) {
          SENSCORD_LOG_ERROR("faild remove socket file: errno=%d", errno);
      }
      socket_path_.clear();
    }
  }
  return Status::OK();
}

/**
 * @brief Connect to the target.
 * @param[in] (param) The informations of the connected target.
 * @return Status object.
 */
Status UdsConnection::Connect(const std::string& param) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  sockaddr_un addr_un = {};
  socklen_t addr_size = 0;
  Status status = GetLocalAddress(param, &addr_un, &addr_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  if (addr_un.sun_path[0] != '\0') {
    SENSCORD_LOG_DEBUG("connect addr: %s", &addr_un.sun_path[0]);
  } else {
    SENSCORD_LOG_DEBUG("connect addr: (abstract) %s", &addr_un.sun_path[1]);
  }

  int32_t ret = connect(socket_,
      reinterpret_cast<sockaddr*>(&addr_un), addr_size);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to connect: errno=%d", errno);
  }
  return Status::OK();
}

/**
 * @brief Bind as the server.
 * @param[in] (param) The informations of the binding.
 * @return Status object.
 */
Status UdsConnection::Bind(const std::string& param) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  sockaddr_un addr_un = {};
  socklen_t addr_size = 0;
  Status status = GetLocalAddress(param, &addr_un, &addr_size);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  if (addr_un.sun_path[0] != '\0') {
    SENSCORD_LOG_DEBUG("bind addr: %s", &addr_un.sun_path[0]);
  } else {
    SENSCORD_LOG_DEBUG("bind addr: (abstract) %s", &addr_un.sun_path[1]);
  }

  int32_t ret = bind(socket_,
      reinterpret_cast<sockaddr*>(&addr_un), addr_size);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to bind: errno=%d", errno);
  }

  // Save the path for file deletion.
  socket_path_ = addr_un.sun_path;

  return Status::OK();
}

/**
 * @brief Start to listen the connection.
 * @return Status object.
 */
Status UdsConnection::Listen() {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  int32_t ret = listen(socket_, kBacklogSize);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to listen: errno=%d", errno);
  }
  return Status::OK();
}

/**
 * @brief Accept the incoming connection.
 * @param[out] (new_connection) The new connection.
 * @param[out] (is_same_system) Whether new connection is on same system.
 * @return Status object.
 */
Status UdsConnection::Accept(
    Connection** new_connection, bool* is_same_system) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }
  if (new_connection == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "new_connection is null");
  }

  int32_t socket_fd = accept(socket_, NULL, 0);
  if (socket_fd < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to accept: errno=%d", errno);
  }

  // create new connection
  UdsConnection* connection = new UdsConnection(socket_fd);
  connection->Open(arguments_);
  *new_connection = connection;

  if (is_same_system) {
    *is_same_system = true;
  }
  return Status::OK();
}

/**
 * @brief Send the message to the connected target.
 * @param[in] (msg) The sending message.
 * @return Status object.
 */
Status UdsConnection::Send(const Message& msg) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  // create message
  serialize::SocketMessageBuffer serialized_msg(
      buffer_chunk_size_, buffer_write_threshold_);
  Status status = connection::SerializeMessage(msg, &serialized_msg);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  uint32_t send_msg_size = static_cast<uint32_t>(
      sizeof(connection::Header) + serialized_msg.size());

  connection::Header header = {};
  osal::OSMemcpy(
      &header.signature, sizeof(header.signature),
      connection::kHeaderSignature, sizeof(connection::kHeaderSignature));
  header.total_size = send_msg_size;

  // Prepare the message.
  std::vector<iovec> iov_list;
  iov_list.reserve(serialized_msg.GetList().size() + 1);
  {
    iovec tmp = {};
    tmp.iov_base = &header;
    tmp.iov_len = sizeof(header);
    iov_list.push_back(tmp);
  }
  for (std::vector<osal::OSSocketMessage>::const_iterator
      itr = serialized_msg.GetList().begin(),
      end = serialized_msg.GetList().end(); itr != end; ++itr) {
    iovec tmp = {};
    tmp.iov_base = itr->buffer;
    tmp.iov_len = itr->buffer_size;
    iov_list.push_back(tmp);
  }

  msghdr send_msg = {};
  send_msg.msg_iov = &iov_list[0];
  send_msg.msg_iovlen = iov_list.size();

  int32_t flags = 0;
  flags |= MSG_NOSIGNAL;  // Do not generate SIGPIPE.

  // send message
  osal::OSLockMutex(mutex_send_);
  ssize_t sent_size = sendmsg(socket_, &send_msg, flags);
  osal::OSUnlockMutex(mutex_send_);

  if ((sent_size < 0) || (static_cast<size_t>(sent_size) != send_msg_size)) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseBusy,
        "failed to send: errno=%d, sendsize=%" PRIu32 ", sent=%" PRIdS,
        errno, send_msg_size, sent_size);
  }

  // for debug
  SENSCORD_LOG_DEBUG("send msg: size=%" PRIdS, send_msg_size);
  return Status::OK();
}

/**
 * @brief Receive the message from the connected target.
 * @param[out] (msg) The receiving message.
 * @return Status object.
 */
Status UdsConnection::Recv(Message* msg) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }
  if (msg == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "msg is null");
  }

  int64_t timeout_nsec = -1;
  if (receive_timeout_msec_ != 0) {
    timeout_nsec = static_cast<int64_t>(receive_timeout_msec_) * 1000000;
  }

  osal::OSSocket* sock = GetOSSocket(socket_);
  connection::Header header;
  Status status = connection::FindHeader(sock, &header, timeout_nsec);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  uint32_t total_size = header.total_size;
  if (total_size <= 8) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAborted, "invalid message size");
  }

  uint32_t payload_size =
      static_cast<uint32_t>(total_size - sizeof(connection::Header));
  std::vector<uint8_t> payload(payload_size);
  status = connection::ReceiveWithTimeout(
      sock, &payload[0], &payload_size, timeout_nsec);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  // for debug
  SENSCORD_LOG_DEBUG("recv msg: size=%" PRIdS, payload_size);

  // Deserialize the received message.
  status = connection::DeserializeMessage(&payload[0], payload.size(), msg);
  return SENSCORD_STATUS_TRACE(status);
}

/**
 * @brief Releases message data generated by Recv function.
 * @param[in] (msg_header) Message header.
 * @param[in] (msg_data) Pointer to message data to release.
 * @return Status object.
 */
Status UdsConnection::ReleaseMessage(
    const MessageHeader& msg_header, void* msg_data) const {
  return connection::ReleaseMessage(msg_header, msg_data);
}

/**
 * @brief Get raw data for SendFrame of the server.
 * @param[in] (channel) Channel object.
 * @param[out] (rawdata) Information of the raw data to send.
 * @return Status object.
 */
Status UdsConnection::GetChannelRawData(
    const Channel* channel, ChannelRawDataInfo* rawdata) const {
  if (rawdata == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "rawdata is null");
  }

  MemoryContained rawdata_memory = {};
  Status status = channel->GetRawDataMemory(&rawdata_memory);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  if (rawdata_memory.memory == NULL) {
    // Do nothing.
    return Status::OK();
  }

  MemoryAllocator* allocator = rawdata_memory.memory->GetAllocator();
  if (allocator->IsMemoryShared()) {
    // serialize rawdata informations.
    rawdata->delivering_mode = kDeliverAddressSizeOnly;
    status = allocator->Serialize(rawdata_memory, &rawdata->rawdata);
    SENSCORD_STATUS_TRACE(status);
  } else {
    // all copy.
    rawdata->delivering_mode = kDeliverAllData;
    if (rawdata_memory.size > 0) {
      // copy if data contained.
      rawdata->rawdata.reserve(rawdata_memory.size);
      uint8_t* src = reinterpret_cast<uint8_t*>(
          rawdata_memory.memory->GetAddress()) + rawdata_memory.offset;
      rawdata->rawdata.assign(src, src + rawdata_memory.size);
    }
  }
  return status;
}

/**
 * @brief Wait to be readable this connection.
 * @param[in] (timeout) Nanoseconds for waiting.
 * @return If be readable then return ok.
 */
Status UdsConnection::WaitReadable(uint64_t timeout) {
  if (socket_ == -1) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(socket_, &rfds);

  timeval tval = ToTimeval(timeout);

  int32_t ret = select(socket_ + 1, &rfds, NULL, NULL, &tval);
  if (ret == 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseTimeout,
        "timeout to wait readable");
  } else if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseAborted,
        "failed to wait readable: %d", errno);
  }
  return Status::OK();
}

/**
 * @brief Constructor.
 */
UdsConnection::UdsConnection()
    : socket_(-1), mutex_send_(), receive_timeout_msec_(),
      buffer_chunk_size_(), buffer_write_threshold_() {
  osal::OSCreateMutex(&mutex_send_);
}

/**
 * @brief Constructor with the created socket.
 * @param[in] (socket) The created socket.
 */
UdsConnection::UdsConnection(int32_t socket)
    : socket_(socket), mutex_send_(), receive_timeout_msec_(),
      buffer_chunk_size_(), buffer_write_threshold_() {
  osal::OSCreateMutex(&mutex_send_);
}

/**
 * @brief Destructor.
 */
UdsConnection::~UdsConnection() {
  Close();
  osal::OSDestroyMutex(mutex_send_);
}

}   // namespace senscord
