
// Copyright 2023 Two Six Technologies
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "ChannelId.h"
#include "RaceHandle.h"

namespace Raceboat {

class Core;
class Race;

using LinkAddress = std::string;
using OpHandle = RaceHandle;

enum class ApiStatus {
  INVALID,
  OK,
  CLOSING,
  CHANNEL_INVALID,
  INVALID_ARGUMENT,
  PLUGIN_ERROR,
  INTERNAL_ERROR,
  CANCELLED
};

struct ReceiveOptions {
public:
  ChannelId recv_channel;
  ChannelId send_channel;
  ChannelId alt_channel;
  LinkAddress recv_address;
  std::string send_role;
  std::string recv_role;
  int timeout_ms = 0;
  bool multi_channel = false;
};

struct SendOptions {
public:
  ChannelId recv_channel;
  ChannelId send_channel;
  ChannelId alt_channel;
  LinkAddress send_address;
  std::string send_role;
  std::string recv_role;
  int timeout_ms = 0;
};

struct ResumeOptions {
public:
  ChannelId recv_channel;
  ChannelId send_channel;
  LinkAddress send_address;
  std::string send_role;
  LinkAddress recv_address;
  std::string recv_role;
  std::string package_id;
  int timeout_ms = 0;
};

struct BootstrapConnectionOptions {
public:
  ChannelId init_send_channel;
  ChannelId init_recv_channel;
  ChannelId final_send_channel;
  ChannelId final_recv_channel;
  LinkAddress init_send_address;
  LinkAddress init_recv_address;
  std::string init_send_role;
  std::string init_recv_role;
  std::string final_send_role;
  std::string final_recv_role;
  int timeout_ms = 0;
};
std::string recvOptionsToString(const ReceiveOptions &recvOptions);
std::string sendOptionsToString(const SendOptions &sendOptions);
std::string resumeOptionsToString(const ResumeOptions &resumeOptions);
std::string bootstrapConnectionOptionsToString(const BootstrapConnectionOptions &bootstrapConnectionOptions);
std::string apiStatusToString(const ApiStatus status);


struct ConduitProperties {
public:
  std::string package_id;
  ChannelId recv_channel;
  std::string recv_role;
  LinkAddress recv_address;
  ChannelId send_channel;
  std::string send_role;
  LinkAddress send_address;
  int timeout_ms = 0;
};

class Conduit {
public:
  Conduit(std::shared_ptr<Core> core, OpHandle handle, ConduitProperties properties);
  Conduit() : handle(NULL_RACE_HANDLE) {}
  Conduit(const Conduit &that);
  virtual ~Conduit();

  OpHandle getHandle();  // debug

  std::pair<ApiStatus, std::vector<uint8_t>> read(int timeoutTimestamp = BLOCKING_READ);
  std::pair<ApiStatus, std::string> read_str();
  ApiStatus write(std::vector<uint8_t> message);
  ApiStatus write_str(const std::string &message);
  ApiStatus close();
  ApiStatus cancelRead();
  ConduitProperties getConduitProperties();

  static const int BLOCKING_READ = 0;

private:
  std::shared_ptr<Core> core;
  OpHandle handle;
  ConduitProperties properties;
};

class ReceiveObject {
public:
  ReceiveObject();
  ReceiveObject(std::shared_ptr<Core> core, OpHandle handle);

  /**
   * @brief Blocks until a package is received. Returns the message received.
   *
   * A Receive object may have receive called on it multiple times. Each time it
   * will return a separate message, potentially from different clients.
   *
   * @return std::tuple<ApiStatus, std::vector<uint8_t>, RespondObject> A status
   * to check in case of an error, and the bytes of the received message.
   */
  std::pair<ApiStatus, std::vector<uint8_t>> receive();

  /**
   * @brief Blocks until a package is received. Returns the message received.
   * Same as `receive()`, but returns a string instead of a vector of bytes.
   *
   * @return std::tuple<ApiStatus, std::string, RespondObject> A status code to
   * check in case of an error, and the received message as a string.
   */
  std::pair<ApiStatus, std::string> receive_str();

  /**
   * @brief Tell the internal link to stop receiving packages
   *
   * @return ApiStatus A status code to check in case of an error
   */
  ApiStatus close();

private:
  std::shared_ptr<Core> core;
  OpHandle handle;
};

class RespondObject {
public:
  RespondObject(std::shared_ptr<Core> core, SendOptions sendOptions);

  /**
   * @brief Reply to a node that previous sent a message
   *
   * @return ApiStatus A status code to check in case of an error
   */
  ApiStatus respond(std::vector<uint8_t> response);
  ApiStatus respond_str(std::string response);

private:
  std::shared_ptr<Core> core;
  SendOptions sendOptions;
};

class ReceiveRespondObject {
public:
  ReceiveRespondObject(std::shared_ptr<Core> core, OpHandle handle,
                       ReceiveOptions options);

  /**
   * @brief Blocks until a package is received. Returns the message received and
   * an object that may be used to respond to the node that sent the message
   *
   * A ReceiveRespond object may have receive called on it multiple times. Each
   * time it will return a separate message, potentially from different clients.
   * The respond object returned by each call may be used to respond to the
   * corresponding client, even if a message was received from a different
   * client in between receiving and responding.
   *
   * @return std::tuple<ApiStatus, std::vector<uint8_t>, RespondObject> A status
   * to check in case of an error, the bytes of the received message, and an
   * object that can be used to respond to the sender of the message.
   */
  std::tuple<ApiStatus, std::vector<uint8_t>, RespondObject> receive();

  /**
   * @brief Blocks until a package is received. Returns the message received and
   * an object that may be used to respond to the node that sent the message.
   * Same as `receive()`, but returns a string instead of a vector of bytes.
   *
   * @return std::tuple<ApiStatus, std::string, RespondObject> A status code to
   * check in case of an error, the received message as a string, and an object
   * that can be used to respond to the sender of the message.
   */
  std::tuple<ApiStatus, std::string, RespondObject> receive_str();

  /**
   * @brief Tell the internal link to stop receiving packages
   *
   * @return ApiStatus A status code to check in case of an error
   */
  ApiStatus close();

private:
  std::shared_ptr<Core> core;
  OpHandle handle;
  ReceiveOptions recvOptions;
};

class AcceptObject {
public:
  AcceptObject(std::shared_ptr<Core> core, OpHandle handle);
  AcceptObject() {}
  std::pair<ApiStatus, Conduit> accept();

private:
  std::shared_ptr<Core> core;
  OpHandle handle;
};

class ChannelParamStore {
  friend class UserInput;

private:
  std::unordered_map<std::string, std::string> params;

public:
  void setChannelParam(std::string key, std::string value);
};

class Race {
private:
  std::shared_ptr<Core> core;

public:
  Race(std::string race_dir, ChannelParamStore params);
  Race(std::shared_ptr<Core> core);
  virtual ~Race();

  /**
   * @brief Open the server side of a unidirectional connection and start
   * listening for messages
   *
   * To get the bytes of received messages, call .receive on the returned
   * ReceiveObject.
   *
   * @param options The options to use when opening. The send_channel field must
   * be set.
   * @return std::pair<LinkAddress, ReceiveObject> The link address to use to
   * connect to this node and the ReceiveObject to use to get received messages
   */
  std::tuple<ApiStatus, LinkAddress, ReceiveObject>
  receive(ReceiveOptions options);

  /**
   * @brief Open the server side of a bidirection oneshot connection and start
   * listening for messages
   *
   * @param options The options to use when opening. The send_channel and
   * recv_channel fields must be set.
   * @return std::pair<LinkAddress, ReceiveRespondObject> std::pair<LinkAddress,
   * ReceiveObject> The link address to use to connect to this node and the
   * ReceiveRespondObject to use to get and respond to received messages
   */
  std::tuple<ApiStatus, LinkAddress, ReceiveRespondObject>
  receive_respond(ReceiveOptions options);

  /**
   * @brief Open the server side of a long-lived bidirection connection and
   * start listening for
   *
   * @param options The options to use when opening. The send_channel field must
   * be set.
   * @return std::pair<LinkAddress, AcceptObject> The link address to use to
   * connect to this node and the AcceptObject to use to accept connection
   * requests
   */
  std::tuple<ApiStatus, LinkAddress, AcceptObject>
  listen(ReceiveOptions options);

  /**
   * @brief Open the server side of a long-lived bidirection connection and
   * start listening for
   *
   * @param options The options to use when opening. The send_channel field must
   * be set.
   * @return std::pair<LinkAddress, AcceptObject> The link address to use to
   * connect to this node and the AcceptObject to use to accept connection
   * requests
   */
  std::tuple<ApiStatus, LinkAddress, AcceptObject>
  bootstrap_listen(BootstrapConnectionOptions options);

  /**
   * @brief Send a unidirectional message to a server
   *
   * @param options The options to use when sending. The send_channel and
   * send_addr fields must be set.
   * @param bytes The message to send
   */
  ApiStatus send(SendOptions options, std::vector<uint8_t> bytes);

  /**
   * @brief Send a unidirectional message to a server
   *
   * @param options The options to use when sending. The send_channel and
   * send_addr fields must be set.
   * @param message The message to send
   */
  ApiStatus send_str(SendOptions options, std::string message);

  /**
   * @brief Send a message to a server and wait for a response.
   *
   * @param options The options to use when sending. The send_channel,
   * send_addr, and recv_channel fields must be set.
   * @param bytes The message to send
   * @return std::vector<uint8_t> the message returned by the server
   */
  std::pair<ApiStatus, std::vector<uint8_t>>
  send_receive(SendOptions options, std::vector<uint8_t> bytes);

  /**
   * @brief Send a message to a server and wait for a response.
   *
   * @param options The options to use when sending. The send_channel,
   * send_addr, and recv_channel fields must be set.
   * @param message The message to send
   * @return std::string the message returned by the server
   */
  std::pair<ApiStatus, std::string> send_receive_str(SendOptions options,
                                                     std::string message);

  /**
   * @brief Create a connection to a server
   *
   * @param options The options to use when sending. The send_channel,
   * send_addr, and recv_channel fields must be set.
   * @param bytes A message to send along with the introduction
   * @return Conduit an object to use to communicate with the server
   */
  std::pair<ApiStatus, Conduit> dial(SendOptions options,
                                              std::vector<uint8_t> bytes);

  /**
   * @brief Resume a connection to a server
   *
   * @param options The options to use to resume the conduit
   * @param bytes A message to send along with the introduction
   * @return Conduit an object to use to communicate with the server
   */
  std::pair<ApiStatus, Conduit> resume(ResumeOptions options);

   /**
   * @brief Create a connection to a server
   *
   * @param options The options to use when sending. The send_channel,
   * send_addr, and recv_channel fields must be set.
   * @param message A message to send along with the introduction
   * @return Conduit an object to use to communicate with the server
   */
  std::pair<ApiStatus, Conduit> dial_str(SendOptions options,
                                                  std::string message);

  /**
   * @brief Create a connection to a server
   *
   * @param options The options to use when sending.
   * @param bytes A message to send along with the introduction
   * @return Conduit an object to use to communicate with the server
   */
  std::pair<ApiStatus, Conduit> bootstrap_dial(BootstrapConnectionOptions options,
                                                            std::vector<uint8_t> bytes);
  
  /**
   * @brief Create a connection to a server
   *
   * @param options The options to use when sending. 
   * @param message A message to send along with the introduction
   * @return Conduit an object to use to communicate with the server
   */
  std::pair<ApiStatus, Conduit> bootstrap_dial_str(BootstrapConnectionOptions options,
                                                            std::string message);
};
} // namespace Raceboat
