
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

#include "race/Race.h"

#include <atomic>
#include <future>
#include <sstream>

#include "Core.h"
#include "helper.h"

namespace Raceboat {

std::string recvOptionsToString(const ReceiveOptions &recvOptions) {
  std::stringstream ss;

  ss << "RecvOptions {";
  ss << "recv_channel: '" << recvOptions.recv_channel << "', ";
  ss << "send_channel: '" << recvOptions.send_channel << "', ";
  ss << "alt_channel: '" << recvOptions.alt_channel << "', ";
  ss << "send_role: '" << recvOptions.send_role << "', ";
  ss << "recv_role: '" << recvOptions.recv_role << "', ";
  ss << "timeout_ms: '" << recvOptions.timeout_ms << "'}";
  return ss.str();
}

std::string sendOptionsToString(const SendOptions &sendOptions) {
  std::stringstream ss;

  ss << "SendOptions {";
  ss << "recv_channel: '" << sendOptions.recv_channel << "', ";
  ss << "send_channel: '" << sendOptions.send_channel << "', ";
  ss << "alt_channel: '" << sendOptions.alt_channel << "', ";
  ss << "send_address: '" << sendOptions.send_address << "', ";
  ss << "send_role: '" << sendOptions.send_role << "', ";
  ss << "recv_role: '" << sendOptions.recv_role << "', ";
  ss << "timeout_ms: '" << sendOptions.timeout_ms << "'}";
  return ss.str();
}

std::string resumeOptionsToString(const ResumeOptions &resumeOptions) {
  std::stringstream ss;

  ss << "ResumeOptions {";
  ss << "send_channel: '" << resumeOptions.send_channel << "', ";
  ss << "recv_channel: '" << resumeOptions.recv_channel << "', ";
  ss << "alt_channel: '" << resumeOptions.alt_channel << "', ";
  ss << "send_address: '" << resumeOptions.send_address << "', ";
  ss << "send_role: '" << resumeOptions.send_role << "', ";
  ss << "recv_address: '" << resumeOptions.recv_address << "', ";
  ss << "recv_role: '" << resumeOptions.recv_role << "', ";
  ss << "timeout_ms: '" << resumeOptions.timeout_ms << "'}";
  return ss.str();
}

std::string bootstrapConnectionOptionsToString(const BootstrapConnectionOptions &bootstrapConnectionOptions) {
  std::stringstream ss;

  ss << "BootstrapConnectionOptions {";
  ss << "init_recv_channel: '" << bootstrapConnectionOptions.init_recv_channel << "', ";
  ss << "init_send_channel: '" << bootstrapConnectionOptions.init_send_channel << "', ";
  ss << "final_recv_channel: '" << bootstrapConnectionOptions.final_recv_channel << "', ";
  ss << "final_send_channel: '" << bootstrapConnectionOptions.final_send_channel << "', ";
  ss << "init_send_address: '" << bootstrapConnectionOptions.init_send_address << "', ";
  ss << "init_recv_address: '" << bootstrapConnectionOptions.init_recv_address << "', ";
  ss << "init_send_role: '" << bootstrapConnectionOptions.init_send_role << "', ";
  ss << "init_recv_role: '" << bootstrapConnectionOptions.init_recv_role << "', ";
  ss << "final_send_role: '" << bootstrapConnectionOptions.final_send_role << "', ";
  ss << "final_recv_role: '" << bootstrapConnectionOptions.final_recv_role << "', ";
  ss << "timeout_seconds: '" << bootstrapConnectionOptions.timeout_seconds << "'}";
  return ss.str();
}

std::string apiStatusToString(const ApiStatus status) {
  switch(status) {
    case ApiStatus::INVALID:
      return "INVALID";
    case ApiStatus::OK:
      return "OK";
    case ApiStatus::CLOSING:
      return "CLOSING";
    case ApiStatus::CHANNEL_INVALID:
      return "CHANNEL_INVALID";
    case ApiStatus::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case ApiStatus::PLUGIN_ERROR:
      return "PLUGIN_ERROR";
    case ApiStatus::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case ApiStatus::TIMEOUT:
      return "TIMEOUT";
    default:
    return "UNKNOWN";
  }
}


Conduit::Conduit(std::shared_ptr<Core> core, OpHandle handle)
    : core(core), handle(handle) {}
Conduit::Conduit(const Conduit &that) {
  core = that.core;
  handle = that.handle;
}

OpHandle Conduit::getHandle() { 
  return handle; 
}

std::pair<ApiStatus, std::vector<uint8_t>> Conduit::read(int timeoutSeconds) {
  TRACE_METHOD(timeoutSeconds);
  std::promise<std::pair<ApiStatus, std::vector<uint8_t>>> promise;
  auto future = promise.get_future();

  if (core == nullptr) {
    return {ApiStatus::INVALID, {}};
  }

  auto response = core->getApiManager().read(
      handle, [&promise](ApiStatus status, std::vector<uint8_t> bytes) {
        promise.set_value({status, std::move(bytes)});
      });
  if (response.status != SDK_OK) {
    return {ApiStatus::INVALID_ARGUMENT, {}};
  }

  if (timeoutSeconds != BLOCKING_READ) { 
    if(std::future_status::ready != future.wait_for(std::chrono::seconds(timeoutSeconds))) {
      helper::logDebug(logPrefix + "timed out");
      return {ApiStatus::TIMEOUT, {}};
    } else {
      return future.get();  
    }
  } else {
    return future.get();
  }
}

std::pair<ApiStatus, std::string> Conduit::read_str() {
  TRACE_METHOD();
  auto [status, bytes] = read();
  std::string str{bytes.begin(), bytes.end()};
  return {status, str};
}

ApiStatus Conduit::write(std::vector<uint8_t> bytes) {
  TRACE_METHOD();
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();

  if (core == nullptr) {
    return ApiStatus::INVALID;
  }

  // Taking promise by reference is fine because we block waiting for the
  // return. If this function gets changed to timeout, this should be changed as
  // well.
  auto response = core->getApiManager().write(
      handle, std::move(bytes),
      [&promise](ApiStatus status) { promise.set_value(status); });
  if (response.status != SDK_OK) {
    return ApiStatus::INVALID_ARGUMENT;
  }

  return future.get();
}

ApiStatus Conduit::write_str(const std::string &message) {
  TRACE_METHOD();
  std::vector<uint8_t> bytes{message.begin(), message.end()};
  return write(bytes);
}

ApiStatus Conduit::close() {
  TRACE_METHOD();
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();
  if (core == nullptr) {
    return ApiStatus::INVALID_ARGUMENT;
  }

  auto response = core->getApiManager().close(
      handle, [&promise](ApiStatus status) { promise.set_value(status); });
  if (response.status != SDK_OK) {
    return ApiStatus::INTERNAL_ERROR;
  }

  return future.get();
}

ReceiveObject::ReceiveObject() : handle(0) {}
ReceiveObject::ReceiveObject(std::shared_ptr<Core> core, OpHandle handle)
    : core(core), handle(handle) {}

std::pair<ApiStatus, std::vector<uint8_t>> ReceiveObject::receive() {
  std::promise<std::pair<ApiStatus, std::vector<uint8_t>>> promise;
  auto future = promise.get_future();

  if (core == nullptr) {
    return {ApiStatus::INVALID, {}};
  }

  // Taking promise by reference is fine because we block waiting for the
  // return. If this function gets changed to timeout, this should be changed as
  // well.
  auto response = core->getApiManager().receive(
      handle, [&promise](ApiStatus status, std::vector<uint8_t> bytes) {
        promise.set_value({status, std::move(bytes)});
      });
  if (response.status != SDK_OK) {
    return {ApiStatus::INVALID_ARGUMENT, {}};
  }

  return future.get();
}

std::pair<ApiStatus, std::string> ReceiveObject::receive_str() {
  auto [status, bytes] = receive();
  std::string str{bytes.begin(), bytes.end()};
  return {status, str};
}

ApiStatus ReceiveObject::close() {
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();
  if (core == nullptr) {
    return ApiStatus::INVALID_ARGUMENT;
  }

  auto response = core->getApiManager().close(
      handle, [&promise](ApiStatus status) { promise.set_value(status); });
  if (response.status != SDK_OK) {
    return ApiStatus::INTERNAL_ERROR;
  }

  return future.get();
}

RespondObject::RespondObject(std::shared_ptr<Core> core,
                             SendOptions sendOptions)
    : core(core), sendOptions(sendOptions) {}

//  respond to the package received by the receive call
ApiStatus RespondObject::respond(std::vector<uint8_t> data) {
  TRACE_METHOD();
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();
  core->getApiManager().send(sendOptions, data, [&promise](ApiStatus status) {
    promise.set_value(status);
  });
  return future.get();
}

ApiStatus RespondObject::respond_str(std::string response) {
  std::vector<uint8_t> bytes{response.begin(), response.end()};
  return respond(bytes);
}

ReceiveRespondObject::ReceiveRespondObject(std::shared_ptr<Core> core,
                                           OpHandle handle,
                                           ReceiveOptions options)
    : core(core), handle(handle), recvOptions(options) {}

//  blocks until a package is received. This object may be reused after
//  responding.
std::tuple<ApiStatus, std::vector<uint8_t>, RespondObject>
ReceiveRespondObject::receive() {
  std::promise<std::tuple<ApiStatus, std::vector<uint8_t>, LinkAddress>>
      promise;
  auto future = promise.get_future();

  if (core == nullptr) {
    return {ApiStatus::INVALID, {}, RespondObject{nullptr, {}}};
  }

  // Taking promise by reference is fine because we block waiting for the
  // return. If this function gets changed to timeout, this should be changed as
  // well.
  auto response = core->getApiManager().receiveRespond(
      handle, [&promise](ApiStatus status, std::vector<uint8_t> bytes,
                         LinkAddress respondAddress) {
        promise.set_value({status, std::move(bytes), respondAddress});
      });
  if (response.status != SDK_OK) {
    return {ApiStatus::INVALID_ARGUMENT, {}, RespondObject{nullptr, {}}};
  }

  auto [status, data, respondAddress] = future.get();
  if (status != ApiStatus::OK) {
    return {status, data, {nullptr, {}}};
  }

  SendOptions sendOptions;
  sendOptions.send_channel = recvOptions.send_channel;
  sendOptions.send_role = recvOptions.send_role;
  sendOptions.send_address = respondAddress;
  RespondObject responder(core, sendOptions);
  return {status, data, responder};
}

std::tuple<ApiStatus, std::string, RespondObject>
ReceiveRespondObject::receive_str() {
  auto [status, bytes, reply_object] = receive();
  std::string str{bytes.begin(), bytes.end()};
  return {status, str, reply_object};
}

ApiStatus ReceiveRespondObject::close() {
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();
  if (core == nullptr) {
    return ApiStatus::INVALID_ARGUMENT;
  }

  auto response = core->getApiManager().close(
      handle, [&promise](ApiStatus status) { promise.set_value(status); });
  if (response.status != SDK_OK) {
    return ApiStatus::INTERNAL_ERROR;
  }

  return future.get();
}

AcceptObject::AcceptObject(std::shared_ptr<Core> core, OpHandle handle)
    : core(core), handle(handle) {}

std::pair<ApiStatus, Conduit> AcceptObject::accept() {
  std::promise<std::pair<ApiStatus, RaceHandle>> promise;
  auto future = promise.get_future();
  if (core == nullptr) {
    return {ApiStatus::INVALID_ARGUMENT, {nullptr, NULL_RACE_HANDLE}};
  }

  auto response = core->getApiManager().accept(
      handle, [&promise](ApiStatus status, RaceHandle _handle) {
        promise.set_value({status, _handle});
      });
  if (response.status != SDK_OK) {
    return {ApiStatus::INTERNAL_ERROR, {nullptr, NULL_RACE_HANDLE}};
  }

  auto [status, connHandle] = future.get();
  Conduit connection(core, connHandle);
  return {status, connection};
}

void ChannelParamStore::setChannelParam(std::string key, std::string value) {
  params[key] = value;
}

Race::Race(std::string race_dir, ChannelParamStore params)
    : core(std::make_shared<Core>(race_dir, params)) {}
Race::Race(std::shared_ptr<Core> core) : core(core) {}

Race::~Race() {}

std::tuple<ApiStatus, LinkAddress, ReceiveObject>
Race::receive(ReceiveOptions options) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, LinkAddress, RaceHandle>> promise;
  auto future = promise.get_future();

  // Taking promise by reference is fine because we block waiting for the
  // return. If this function gets changed to timeout, this should be changed as
  // well.
  core->getApiManager().getReceiveObject(
      options,
      [&promise](ApiStatus status, LinkAddress addr, RaceHandle handle) {
        promise.set_value({status, addr, handle});
      });

  auto [status, linkAddr, handle] = future.get();
  ReceiveObject receiver(core, handle);
  return {status, linkAddr, receiver};
}

std::tuple<ApiStatus, LinkAddress, ReceiveRespondObject>
Race::receive_respond(ReceiveOptions options) {
  std::promise<std::tuple<ApiStatus, LinkAddress, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().getReceiveObject(
      options,
      [&promise](ApiStatus status, LinkAddress addr, RaceHandle handle) {
        promise.set_value({status, addr, handle});
      });

  auto [status, linkAddr, handle] = future.get();
  ReceiveRespondObject receiver(core, handle, options);
  return {status, linkAddr, receiver};
}

std::tuple<ApiStatus, LinkAddress, AcceptObject>
Race::listen(ReceiveOptions options) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, LinkAddress, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().listen(
      options,
      [&promise](ApiStatus status, LinkAddress addr, RaceHandle handle) {
        promise.set_value({status, addr, handle});
      });

  auto [status, addr, handle] = future.get();
  AcceptObject receiver(core, handle);
  return {status, addr, receiver};
}

std::tuple<ApiStatus, LinkAddress, AcceptObject>
Race::bootstrap_listen(BootstrapConnectionOptions options) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, LinkAddress, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().bootstrapListen(
      options,
      [&promise](ApiStatus status, LinkAddress addr, RaceHandle handle) {
        promise.set_value({status, addr, handle});
      });

  auto [status, addr, handle] = future.get();
  AcceptObject receiver(core, handle);
  return {status, addr, receiver};
}

ApiStatus Race::send(SendOptions options, std::vector<uint8_t> data) {
  TRACE_METHOD();
  std::promise<ApiStatus> promise;
  auto future = promise.get_future();
  core->getApiManager().send(options, data, [&promise](ApiStatus status) {
    promise.set_value(status);
  });
  return future.get();
}

ApiStatus Race::send_str(SendOptions options, std::string message) {
  TRACE_METHOD();
  std::vector<uint8_t> bytes{message.begin(), message.end()};
  return send(options, bytes);
}

std::pair<ApiStatus, std::vector<uint8_t>>
Race::send_receive(SendOptions options, std::vector<uint8_t> data) {
  TRACE_METHOD();
  std::promise<std::pair<ApiStatus, std::vector<uint8_t>>> promise;
  auto future = promise.get_future();
  core->getApiManager().sendReceive(
      options, data,
      [&promise](ApiStatus status, std::vector<uint8_t> response) {
        promise.set_value({status, response});
      });
  return future.get();
}

std::pair<ApiStatus, std::string> Race::send_receive_str(SendOptions options,
                                                         std::string message) {
  TRACE_METHOD();
  std::vector<uint8_t> bytes{message.begin(), message.end()};
  auto [status, message_bytes] = send_receive(options, bytes);
  std::string str{message_bytes.begin(), message_bytes.end()};
  return {status, str};
}

std::pair<ApiStatus, Conduit> Race::dial(SendOptions options,
                                                  std::vector<uint8_t> bytes) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().dial(options, bytes,
                             [&promise](ApiStatus status, RaceHandle handle) {
                               promise.set_value({status, handle});
                             });

  auto [status, handle] = future.get();
  Conduit receiver(core, handle);
  return {status, receiver};
}

std::pair<ApiStatus, Conduit> Race::dial_str(SendOptions options,
                                                      std::string message) {
  TRACE_METHOD();
  std::vector<uint8_t> bytes{message.begin(), message.end()};
  return dial(options, bytes);
}

  
std::pair<ApiStatus, Conduit> Race::resume(ResumeOptions options) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().resume(options,
                             [&promise](ApiStatus status, RaceHandle handle) {
                               promise.set_value({status, handle});
                             });

  auto [status, handle] = future.get();
  Conduit receiver(core, handle);
  return {status, receiver};
}


std::pair<ApiStatus, Conduit> Race::bootstrap_dial(BootstrapConnectionOptions options,
                                                  std::vector<uint8_t> bytes) {
  TRACE_METHOD();
  std::promise<std::tuple<ApiStatus, RaceHandle>> promise;
  auto future = promise.get_future();

  core->getApiManager().bootstrapDial(options, bytes,
                             [&promise](ApiStatus status, RaceHandle handle) {
                               promise.set_value({status, handle});
                             });

  auto [status, handle] = future.get();
  Conduit receiver(core, handle);
  return {status, receiver};
}

std::pair<ApiStatus, Conduit> Race::bootstrap_dial_str(BootstrapConnectionOptions options,
                                                      std::string message) {
  TRACE_METHOD();
  std::vector<uint8_t> bytes{message.begin(), message.end()};
  return bootstrap_dial(options, bytes);
}

} // namespace Raceboat
