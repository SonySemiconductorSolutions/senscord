/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senscord/connection/tcp_connection.h"
#include <string>
#include <sstream>
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
SENSCORD_REGISTER_CONNECTION(senscord::TcpConnection)

namespace {

const char* kArgumentBufferChunkSize = "buffer_chunk_size";
const char* kArgumentBufferWriteSizeThreshold = "buffer_write_size_threshold";
const char* kArgumentReuseAddr = "reuse_addr";
const char* kArgumentValueOn = "on";
const char* kArgumentConnectTimeout = "connect_timeout_msec";
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

}  // namespace

namespace senscord {

// backlog size for listen.
static const int32_t kBacklogSize = 3;

/**
 * @brief Open the connection.
 * @param[in] (arguments) connection arguments.
 * @return Status object.
 */
Status TcpConnection::Open(
    const std::map<std::string, std::string>& arguments) {
  arguments_ = arguments;
  ParseArguments();
  return Open();
}

/**
 * @brief Open the connection.
 * @return Status object.
 */
Status TcpConnection::Open() {
  if (socket_) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "already opened");
  }

  int32_t ret = osal::OSCreateSocket(osal::kSocketTypeInetTcp, &socket_);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to create socket");
  }

  EnableNoDelay();
  return Status::OK();
}

/**
 * @brief Close the connection.
 * @return Status object.
 */
Status TcpConnection::Close() {
  if (socket_) {
    // force shutdown
    osal::OSShutdownSocket(socket_, osal::kShutdownBoth);

    // close
    int32_t ret = osal::OSDestroySocket(socket_);
    if (ret < 0) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseInvalidOperation,
          "failed to destroy socket: %" PRIx32,
          ret);
    }
    socket_ = NULL;
  }
  return Status::OK();
}

/**
 * @brief Connect to the target.
 * @param[in] (param) The informations of the connected target.
 * @return Status object.
 */
Status TcpConnection::Connect(const std::string& param) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }
  osal::OSSocketAddressInet addr = GetAddress(param);
  SENSCORD_LOG_DEBUG("connect addr: 0x%" PRIx32 ":%" PRIu32,
      addr.address, addr.port);
  int32_t ret = 0;
  if (connect_timeout_msec_ == 0) {
    ret = osal::OSConnectSocket(socket_, addr);
  } else {
    ret = osal::OSConnectSocket(
        socket_, addr, static_cast<uint64_t>(connect_timeout_msec_) * 1000000);
  }
  if (ret < 0) {
    if (osal::error::IsTimeout(ret)) {
      return SENSCORD_STATUS_FAIL(kStatusBlockCore,
          Status::kCauseTimeout, "connect timed out: 0x%" PRIx32, ret);
    }
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to connect: 0x%" PRIx32, ret);
  }
  is_same_system_ = IsSameSystem(addr);
  return Status::OK();
}

/**
 * @brief Bind as the server.
 * @param[in] (param) The informations of the binding.
 * @return Status object.
 */
Status TcpConnection::Bind(const std::string& param) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  // Set the ReuseAddr option before binding.
  osal::OSSetSocketReuseAddr(socket_, reuse_addr_);

  osal::OSSocketAddressInet addr = GetAddress(param);
  SENSCORD_LOG_DEBUG("bind addr: 0x%" PRIx32 ":%" PRIu32,
      addr.address, addr.port);
  int32_t ret = osal::OSBindSocket(socket_, addr);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to bind: 0x%" PRIx32, ret);
  }
  address_ = addr;
  return Status::OK();
}

/**
 * @brief Start to listen the connection.
 * @return Status object.
 */
Status TcpConnection::Listen() {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  osal::OSListenSocket(socket_, kBacklogSize);
  return Status::OK();
}

/**
 * @brief Accept the incoming connection.
 * @param[out] (new_connection) The new connection.
 * @param[out] (is_same_system) Whether new connection is on same system.
 * @return Status object.
 */
Status TcpConnection::Accept(
    Connection** new_connection, bool* is_same_system) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }
  if (new_connection == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidArgument, "new_connection is null");
  }

  osal::OSSocket* new_sock = NULL;
  osal::OSSocketAddressInet address = {};
  int32_t ret = osal::OSAcceptSocket(socket_, &new_sock, &address);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "failed to accept: 0x%" PRIx32, ret);
  }

  bool same_system = IsSameSystem(address);

  // create new connection
  TcpConnection* connection = new TcpConnection(
      new_sock, address, same_system);
  connection->Open(arguments_);
  *new_connection = connection;

  if (is_same_system) {
    *is_same_system = same_system;
  }
  return Status::OK();
}

/**
 * @brief Send the message to the connected target.
 * @param[in] (msg) The semding message.
 * @return Status object.
 */
Status TcpConnection::Send(const Message& msg) {
  if (socket_ == NULL) {
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
  header.total_size = osal::OSHtonl(send_msg_size);

  // Prepare the message.
  std::vector<osal::OSSocketMessage> send_msg;
  send_msg.reserve(serialized_msg.GetList().size() + 1);
  {
    // connection header.
    osal::OSSocketMessage tmp = {};
    tmp.buffer = &header;
    tmp.buffer_size = sizeof(header);
    send_msg.push_back(tmp);
  }
  send_msg.insert(send_msg.end(),
                  serialized_msg.GetList().begin(),
                  serialized_msg.GetList().end());

  // send message
  size_t sent_size = 0;

  osal::OSLockMutex(mutex_send_);
  int32_t ret = osal::OSSendMsgSocket(socket_, send_msg, NULL, &sent_size);
  osal::OSUnlockMutex(mutex_send_);

  if ((ret < 0) || (sent_size != send_msg_size)) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseBusy,
        "failed to send:"
        " return=0x%" PRIx32 ", sendsize=%" PRIu32 ", sent=%" PRIdS,
        ret, send_msg_size, sent_size);
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
Status TcpConnection::Recv(Message* msg) {
  if (socket_ == NULL) {
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

  connection::Header header;
  Status status = connection::FindHeader(socket_, &header, timeout_nsec);
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }

  uint32_t total_size = osal::OSNtohl(header.total_size);
  if (total_size <= 8) {
    return SENSCORD_STATUS_FAIL(
        kStatusBlockCore, Status::kCauseAborted, "invalid message size");
  }

  uint32_t payload_size =
      static_cast<uint32_t>(total_size - sizeof(connection::Header));
  std::vector<uint8_t> payload(payload_size);
  status = connection::ReceiveWithTimeout(
      socket_, &payload[0], &payload_size, timeout_nsec);
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
Status TcpConnection::ReleaseMessage(
    const MessageHeader& msg_header, void* msg_data) const {
  return connection::ReleaseMessage(msg_header, msg_data);
}

/**
 * @brief Get raw data for SendFrame of the server.
 * @param[in] (channel) Channel object.
 * @param[out] (rawdata) Information of the raw data to send.
 * @return Status object.
 */
Status TcpConnection::GetChannelRawData(
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
  if (allocator->IsMemoryShared() && is_same_system_) {
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
 * @param[in] (timeout) Nanoseconds for wating.
 * @return If be readable then return ok.
 */
Status TcpConnection::WaitReadable(uint64_t timeout) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  std::vector<osal::OSSocket*> readable;
  readable.push_back(socket_);
  int32_t ret = osal::OSRelativeTimedSelectSocket(
      &readable, NULL, NULL, timeout);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseTimeout,
        "timeout to wait readable");
  }
  return Status::OK();
}

#if 0
/**
 * @brief Wait to be writable this connection.
 * @param[in] (timeout) Nanoseconds for wating.
 * @return If be writable then return ok.
 */
Status TcpConnection::WaitWritable(uint64_t timeout) {
  if (socket_ == NULL) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        Status::kCauseInvalidOperation, "not opened yet");
  }

  std::vector<osal::OSSocket*> writable;
  writable.push_back(socket_);
  int32_t ret = osal::OSRelativeTimedSelectSocket(
      NULL, &writable, NULL, timeout);
  if (ret < 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore, Status::kCauseTimeout,
        "timeout to wait writable");
  }
  return Status::OK();
}
#endif

/**
 * @brief Set rules for reuse of bind address.
 *
 * When using this function, it must be called before Bind().
 *
 * @param[in] (enable) true: Enable reuse.
 */
void TcpConnection::SetReuseAddr(bool enable) {
  reuse_addr_ = enable;
}

/**
 * @bbrief Get the address from parameter.
 * @param[in] (param) The parameter of bind or connect.
 * @return The address for osal socket.
 */
osal::OSSocketAddressInet TcpConnection::GetAddress(
    const std::string& param) const {
  osal::OSSocketAddressInet addr = {osal::kOSInAddrAny, 0};
  if (!param.empty()) {
    std::string ipadr;

    // split ipadr and port
    {
      std::stringstream ss(param);
      std::getline(ss, ipadr, ':');
      ss >> addr.port;
      addr.port = osal::OSHtons(addr.port);
    }

    // split ipadr
    if (!ipadr.empty()) {
      osal::OSInetAton(ipadr.c_str(), &addr.address);
    }
  }
  return addr;
}

/**
 * @brief Check whether incoming address is on same system.
 * @param[in] (incoming_address) The incoming address.
 * @return True means the address is on same system.
 */
bool TcpConnection::IsSameSystem(
    const osal::OSSocketAddressInet& incoming_address) const {
  // loopback or same address
  if (incoming_address.address == osal::OSHtonl(osal::kOSInAddrLoopback)) {
    return true;
  }
  std::vector<osal::OSSocketAddressInet> addr_list;
  osal::OSGetInetAddressList(&addr_list);
  for (std::vector<osal::OSSocketAddressInet>::const_iterator
      itr = addr_list.begin(), end = addr_list.end(); itr != end; ++itr) {
    if (incoming_address.address == itr->address) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Enable the TCP_NODELAY.
 */
void TcpConnection::EnableNoDelay() const {
  int32_t ret = osal::OSSetSocketTcpNoDelay(socket_, true);
  if (ret < 0) {
    SENSCORD_LOG_WARNING("failed to set TCP_NODELAY: ret=%" PRIx32, ret);
  }
}

/**
 * @brief Parse arguments.
 */
void TcpConnection::ParseArguments() {
  buffer_chunk_size_ = GetArgumentUint32(
      arguments_, kArgumentBufferChunkSize, 0);
  buffer_write_threshold_ = GetArgumentUint32(
      arguments_, kArgumentBufferWriteSizeThreshold, 0);
  connect_timeout_msec_ = GetArgumentUint32(
      arguments_, kArgumentConnectTimeout, 0);
  receive_timeout_msec_ = GetArgumentUint32(
      arguments_, kArgumentReceiveTimeout, 0);
  std::map<std::string, std::string>::const_iterator itr =
      arguments_.find(kArgumentReuseAddr);
  if ((itr != arguments_.end()) && (itr->second == kArgumentValueOn)) {
    reuse_addr_ = true;
  }
  SENSCORD_LOG_DEBUG(
      "chunk_size:%" PRIu32 ", write_size:%" PRIu32,
      buffer_chunk_size_, buffer_write_threshold_);
  SENSCORD_LOG_DEBUG(
      "reuse_addr:%s", (reuse_addr_ ? "true" : "false"));
  SENSCORD_LOG_DEBUG(
      "connect_timeout_msec:%" PRIu32 "%s", connect_timeout_msec_,
      (connect_timeout_msec_ == 0) ? " (default)" : "");
  SENSCORD_LOG_DEBUG(
      "receive_timeout_msec:%" PRIu32 "%s", receive_timeout_msec_,
      (receive_timeout_msec_ == 0) ? " (default)" : "");
}

/**
 * @brief Constructor.
 */
TcpConnection::TcpConnection()
    : socket_(), reuse_addr_(), is_same_system_(),
      connect_timeout_msec_(), receive_timeout_msec_(),
      buffer_chunk_size_(), buffer_write_threshold_() {
  osal::OSCreateMutex(&mutex_send_);
}

/**
 * @brief Constructor with the created socket.
 * @param[in] (socket) The created socket.
 * @param[in] (address) The incoming address.
 * @param[in] (is_same_system) Whether the connection is on same system.
 */
TcpConnection::TcpConnection(
    osal::OSSocket* socket,
    const osal::OSSocketAddressInet& address,
    bool is_same_system)
    : socket_(socket), address_(address), reuse_addr_(),
      is_same_system_(is_same_system),
      connect_timeout_msec_(), receive_timeout_msec_(),
      buffer_chunk_size_(), buffer_write_threshold_() {
  osal::OSCreateMutex(&mutex_send_);
  EnableNoDelay();
}

/**
 * @brief Destructor.
 */
TcpConnection::~TcpConnection() {
  Close();
  osal::OSDestroyMutex(mutex_send_);
}

}   // namespace senscord
