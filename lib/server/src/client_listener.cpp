/*
 * SPDX-FileCopyrightText: 2017-2024 Sony Semiconductor Solutions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "client_listener.h"
#include <string>
#include "senscord/osal.h"
#include "server_log.h"

namespace senscord {
namespace server {

/**
 * @brief Constructor.
 * @param[in] (manager) The client adapter manager.
 * @param[in] (connection) The connection interface.
 * @param[in] (address) The bind address.
 */
ClientListenerBase::ClientListenerBase(
    ClientAdapterManager* manager,
    Connection* connection, const std::string& address)
    : manager_(manager), connection_(connection), address_(address),
      thread_(), end_flag_(false) {
}

/**
 * @brief Destructor.
 */
ClientListenerBase::~ClientListenerBase() {
  Stop();
}

/**
 * @brief Start to listen the connection.
 * @return Status object.
 */
Status ClientListenerBase::Start() {
  if (thread_) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseInvalidOperation, "already started");
  }

  Status status;

  // bind and listen.
  status = connection_->Open();
  if (!status.ok()) {
    return SENSCORD_STATUS_TRACE(status);
  }
  status = connection_->Bind(address_);
  if (!status.ok()) {
    connection_->Close();
    return SENSCORD_STATUS_TRACE(status);
  }
  status = connection_->Listen();
  if (!status.ok()) {
    connection_->Close();
    return SENSCORD_STATUS_TRACE(status);
  }

  end_flag_ = false;
  int32_t ret = osal::OSCreateThread(&thread_, ThreadProc, this, NULL);
  if (ret != 0) {
    return SENSCORD_STATUS_FAIL(kStatusBlockCore,
        senscord::Status::kCauseAborted,
        "failed to create listener thread: %" PRIx32, ret);
  }
  return Status::OK();
}

/**
 * @brief Stop to listen the connection.
 * @return Status object.
 */
Status ClientListenerBase::Stop() {
  if (thread_) {
    // stop listener
    end_flag_ = true;
    osal::OSJoinThread(thread_, NULL);
    thread_ = NULL;
  }

  if (connection_ != NULL) {
    connection_->Close();
  }

  return Status::OK();
}

/**
 * @brief Working thread for listen.
 * @param[in] (arg) The instance of client listener.
 * @return Always returns normal.
 */
osal::OSThreadResult ClientListenerBase::ThreadProc(void* arg) {
  if (arg) {
    ClientListenerBase* listener = reinterpret_cast<ClientListenerBase*>(arg);
    listener->Listening();
  }
  return static_cast<osal::OSThreadResult>(0);
}

/**
 * @brief The method for listener threading.
 */
void ClientListenerBase::Listening() {
  SENSCORD_SERVER_LOG_DEBUG("[server] start listening");

  Status status;
  while (!end_flag_) {
    // wait the new connection
    status = connection_->WaitReadable(1000000000);
    if (status.ok()) {
      // incoming conenction
      Connection* new_connection = NULL;
      status = connection_->Accept(&new_connection, NULL);
      if (status.ok()) {
        // incoming new client
        ClientAdapterBase* client = CreateAdapter(manager_, new_connection);
        if (client == NULL) {
          delete new_connection;
          SENSCORD_SERVER_LOG_ERROR(
              "[server] failed to create client");
          continue;
        }
        status = client->Start();
        if (status.ok()) {
          manager_->Register(client);
        } else {
          delete client;
          SENSCORD_SERVER_LOG_ERROR(
              "[server] client initialization failed: %s",
              status.ToString().c_str());
        }
      } else {
        // oops
        SENSCORD_SERVER_LOG_WARNING(status.ToString().c_str());
      }
    } else if (status.cause() != Status::kCauseTimeout) {
      SENSCORD_SERVER_LOG_ERROR("[server] listener connection failed: %s",
          status.ToString().c_str());
      break;
    } else {
      // no connection
    }
  }
  SENSCORD_SERVER_LOG_DEBUG("[server] stop listening");
}

/**
 * @brief Constructor.
 * @param[in] (manager) The client adapter manager.
 * @param[in] (connection) The connection interface.
 * @param[in] (address) The bind address.
 * @param[in] (core) The SDK Core instance.
 * @param[in] (config_manager) The config manager.
 */
ClientListener::ClientListener(
    ClientAdapterManager* manager,
    Connection* connection, const std::string& address,
    Core* core, const ConfigManager& config_manager)
    : ClientListenerBase(manager, connection, address),
      core_(core), config_manager_(config_manager) {}

/**
 * @brief Destructor.
 */
ClientListener::~ClientListener() {}

/**
 * @brief Create client adapter from a new connection.
 * @param[in] (manager) The client adapter manager.
 * @param[in] (new_connection) new connection instance.
 * @return created client adapter.
 */
ClientAdapterBase* ClientListener::CreateAdapter(
    ClientAdapterManager* manager, Connection* new_connection) {
  return new ClientAdapter(manager, new_connection, core_, config_manager_);
}

}   // namespace server
}   // namespace senscord
