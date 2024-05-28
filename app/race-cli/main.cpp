
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

#include <getopt.h>

#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "race/Race.h"
#include "race/common/RaceLog.h"

// #include <boost/asio.hpp>
// #include <boost/bind.hpp>
// #include <boost/shared_ptr.hpp>
// #include <boost/enable_shared_from_this.hpp>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <resolv.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#define READ 0
#define WRITE 1

#define BUF_SIZE 16384
using namespace Raceboat;

enum class Mode : int {
  INVALID,
  SEND_ONESHOT,
  SEND_RECV,
  CLIENT_CONNECT,
  RECV_ONESHOT,
  RECV_RESPOND,
  SERVER_CONNECT,
  CLIENT_BOOTSTRAP_CONNECT,
  SERVER_BOOTSTRAP_CONNECT,
};

struct CmdOptions {
  Mode mode;
  RaceLog::LogLevel log_level;
  std::vector<std::pair<std::string, std::string>> params;

  std::string plugin_path = "/etc/race";

  ChannelId init_recv_channel;
  std::string init_recv_role = "default";
  ChannelId init_send_channel;
  std::string init_send_role = "default";
  ChannelId alt_channel;
  std::string alt_role = "default";
  LinkAddress init_send_address;
  LinkAddress init_recv_address;
  ChannelId final_recv_channel;
  std::string final_recv_role = "default";
  ChannelId final_send_channel;
  std::string final_send_role = "default";
  LinkAddress final_send_address;
  LinkAddress final_recv_address;
  int timeout_ms = 0;
  bool multi_channel = false;

  int num_packages = -1;
};

static std::optional<CmdOptions> parseOpts(int argc, char **argv) {
  static int log_level = RaceLog::LL_INFO;
  static int mode = static_cast<int>(Mode::INVALID);

  static option long_options[] = {
      // logging
      {"debug", no_argument, &log_level, RaceLog::LogLevel::LL_DEBUG},
      {"quiet", no_argument, &log_level, RaceLog::LogLevel::LL_ERROR},

      // connection style
      {"send", no_argument, &mode, static_cast<int>(Mode::SEND_ONESHOT)},
      {"send-recv", no_argument, &mode, static_cast<int>(Mode::SEND_RECV)},
      {"client-connect", no_argument, &mode,
       static_cast<int>(Mode::CLIENT_CONNECT)},
      {"recv", no_argument, &mode, static_cast<int>(Mode::RECV_ONESHOT)},
      {"recv-reply", no_argument, &mode, static_cast<int>(Mode::RECV_RESPOND)},
      {"server-connect", no_argument, &mode,
       static_cast<int>(Mode::SERVER_CONNECT)},
      {"server-bootstrap-connect", no_argument, &mode,
       static_cast<int>(Mode::SERVER_BOOTSTRAP_CONNECT)},
      {"client-bootstrap-connect", no_argument, &mode,
       static_cast<int>(Mode::CLIENT_BOOTSTRAP_CONNECT)},

      // channel selection
      {"recv-channel", required_argument, nullptr, 'R'},
      {"recv-role", required_argument, nullptr, 'r'},
      {"send-channel", required_argument, nullptr, 'S'},
      {"send-role", required_argument, nullptr, 's'},
      {"alt-channel", required_argument, nullptr, 'T'},
      {"alt-role", required_argument, nullptr, 't'},

      {"final-recv-channel", required_argument, nullptr, 'K'},
      {"final-recv-role", required_argument, nullptr, 'k'},
      {"final-send-channel", required_argument, nullptr, 'L'},
      {"final-send-role", required_argument, nullptr, 'l'},

      // destination selection
      {"send-address", required_argument, nullptr, 'a'},
      {"recv-address", required_argument, nullptr, 'e'},

      // race plugin path
      {"dir", required_argument, nullptr, 'd'},

      // param
      {"param", required_argument, nullptr, 'p'},

      // allow send and recv on multiple channels
      {"multi-channel", no_argument, nullptr, 'm'},

      // timeout
      {"timeout", required_argument, nullptr, 'w'},

      // packages
      {"num-packages", required_argument, nullptr, 'n'},

      // help
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  int c;
  CmdOptions opts;

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "R:r:S:s:T:t:a:e:p:mdhwn:", long_options,
                    &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      break;

    case 'R':
      opts.init_recv_channel = optarg;
      break;

    case 'r':
      opts.init_recv_role = optarg;
      break;

    case 'S':
      opts.init_send_channel = optarg;
      break;

    case 's':
      opts.init_send_role = optarg;
      break;

    case 'K':
      opts.final_recv_channel = optarg;
      break;

    case 'k':
      opts.final_recv_role = optarg;
      break;

    case 'L':
      opts.final_send_channel = optarg;
      break;

    case 'l':
      opts.final_send_role = optarg;
      break;

    case 'T':
      opts.alt_channel = optarg;
      break;

    case 't':
      opts.alt_role = optarg;
      break;

    case 'a':
      opts.init_send_address = optarg;
      break;

    case 'e':
      opts.init_recv_address = optarg;
      break;

    case 'd':
      opts.plugin_path = optarg;
      break;

    case 'p': {
      std::string key_value = optarg;
      size_t split_index = key_value.find("=");
      std::string key = key_value.substr(0, split_index);

      if (key.empty()) {
        fprintf(stderr,
                "%s: received empty key for param at argument index %d\n",
                argv[0], optind);
        return std::nullopt;
      }

      if (split_index == std::string::npos ||
          split_index + 1 >= key_value.size()) {
        fprintf(stderr,
                "%s: received empty value for param at argument index %d\n",
                argv[0], optind);
        return std::nullopt;
      }
      std::string value = key_value.substr(split_index + 1, std::string::npos);
      opts.params.push_back({key, value});
      break;
    }

    case 'm':
      opts.multi_channel = true;
      break;

    case 'w':
      try {
        // convert from seconds to milliseconds
        opts.timeout_ms = static_cast<int>(ceil(std::stod(optarg) * 1000));
      } catch (std::exception &e) {
        fprintf(stderr, "%s: received invalid argument for timeout %s\n",
                argv[0], optarg);
        return std::nullopt;
      }
      break;
    case 'n':
      try {
        opts.num_packages = std::stoi(optarg);
      } catch (std::exception &e) {
        fprintf(stderr, "%s: received invalid argument for num_packages %s\n",
                argv[0], optarg);
        return std::nullopt;
      }
      break;

    case '?':
      /* getopt_long already printed an error message. */
      // fallthrough
    case 'h':
      // clang-format off
                printf(
                    "Usage:\n"
                    "\n"
                    "Logging:\n"
                    "    --debug             Enable verbose logging\n"
                    "    --quiet             Disable logging\n"
                    "\n"
                    "Modes:\n"
                    "    --send              Send a message without receiving a response\n"
                    "    --send-recv         Send one message and receive one message in response\n"
                    "    --client-connect    Open a connection to the server\n"
                    "    --recv              Receive one message without responding\n"
                    "    --recv-reply        Receive one message and send a response\n"
                    "    --server-connect    Open a connection to a client\n"
                    "\n"
                    "Channel Selection:\n"
                    "    --recv-channel, -R  Set the channel to receive with\n"
                    "    --recv-role, -r     Set the receive channel's role (default: default)\n"
                    "    --send-channel, -S  Set the channel to send with\n"
                    "    --send-role, -s     Set the send channel's role (default: default)\n"
                    "    --alt-channel, -T   Set the channel used to receive after establishing an initial connection\n"
                    "    --alt-role, -t      Set the alternate channel's role (default: default)\n"
                    "    --send-address, -a  Set the address to send to\n"
                    "    --recv-address, -e  Set the address to listen to (optional, creates a new address by default)\n"
                    "\n"
                    "Channel Parameters:\n"
                    "    --param, -p         Parameters used to specify information necessary for channels to function, e.g. hostname, or account credentials\n"
                    "    --multi-channel, -m Allow send and receive on multiple channels\n"
                    "\n"
                    "Misc:\n"
                    "    --num-packages, -n  Number of packages to receive before closing. -1 for unlimited. (default: -1)\n"
                    "    --timeout, -w       Amount of time to wait before assuming a connection has died\n"
                    "    --dir, -d           The directory to load plugins from (default: /etc/race)\n"
                    "    --help, -h          Print this message\n");
      // clang-format on

      return std::nullopt;

    default:
      // This should never happen. The ? case handles unknown arguments.
      abort();
    }
  }

  opts.mode = static_cast<Mode>(mode);
  opts.log_level = static_cast<RaceLog::LogLevel>(log_level);

  if (optind < argc && optind > 0) {
    fprintf(stderr, "%s: received unexpected argument '%s'\n", argv[0],
            argv[optind]);
    return std::nullopt;
  }

  return {opts};
}

ChannelParamStore getParams(const CmdOptions &opts) {
  ChannelParamStore params;

  for (auto [key, value] : opts.params) {
    RaceLog::logDebug("RaceCli",
                      "Got parameter: '" + key + "' = '" + value + "'", "");
    params.setChannelParam(key, value);
  }
  return params;
}

std::vector<uint8_t> readStdin() {
  std::vector<uint8_t> buffer;
  int c;

  // efficiency isn't a huge concern, so just read one character at a time
  while ((c = getchar()) != EOF) {
    buffer.push_back(c);
  }
  return buffer;
}

int handle_send_oneshot(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  SendOptions send_opt;
  send_opt.send_channel = opts.init_send_channel;
  send_opt.send_role = opts.init_send_role;
  send_opt.send_address = opts.init_send_address;
  send_opt.recv_channel = opts.init_recv_channel;
  send_opt.recv_role = opts.init_recv_role;
  send_opt.alt_channel = opts.alt_channel;

  // TODO: support channel role for alt channel
  // send_opt.alt_role = opts.alt_role;
  send_opt.timeout_ms = opts.timeout_ms;

  auto message = readStdin();
  auto status = race.send(send_opt, message);
  if (status != ApiStatus::OK) {
    RaceLog::logError("RaceCli", "Send failed", "");
    return 1;
  }

  return 0;
}

int handle_recv_oneshot(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  ReceiveOptions recv_opt;
  recv_opt.recv_channel = opts.init_recv_channel;
  recv_opt.recv_role = opts.init_recv_role;

  recv_opt.recv_address = opts.init_recv_address;
  recv_opt.send_channel = opts.init_send_channel;
  recv_opt.send_role = opts.init_send_role;
  recv_opt.alt_channel = opts.alt_channel;
  recv_opt.multi_channel = opts.multi_channel;

  // TODO: support channel role for alt channel
  // recv_opt.alt_role = opts.alt_role;
  recv_opt.timeout_ms = opts.timeout_ms;

  auto [status1, link_addr, listener] = race.receive(recv_opt);
  if (status1 != ApiStatus::OK) {
    RaceLog::logError("RaceCli", "Opening listen failed\n", "");
    return 1;
  }

  printf("Listening on %s\n", link_addr.c_str());

  int packages_remaining = opts.num_packages;
  while (opts.num_packages == -1 || packages_remaining > 0) {
    auto [status2, received_message] = listener.receive_str();
    if (status2 != ApiStatus::OK) {
      RaceLog::logError("RaceCli", "Receive failed\n", "");
      return 1;
    }

    printf("%s\n", received_message.c_str());

    packages_remaining--;
  }

  listener.close();

  return 0;
}

int handle_send_recv(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  SendOptions send_opt;
  send_opt.send_channel = opts.init_send_channel;
  send_opt.send_role = opts.init_send_role;
  send_opt.send_address = opts.init_send_address;
  send_opt.recv_channel = opts.init_recv_channel;
  send_opt.recv_role = opts.init_recv_role;
  send_opt.alt_channel = opts.alt_channel;

  // TODO: support channel role for alt channel
  // send_opt.alt_role = opts.alt_role;
  send_opt.timeout_ms = opts.timeout_ms;

  auto message = readStdin();
  auto [status, received_message] =
      race.send_receive_str(send_opt, {message.begin(), message.end()});
  if (status != ApiStatus::OK) {
    RaceLog::logError("RaceCli", "Send/Receive failed", "");
    return 1;
  }

  printf("%s\n", received_message.c_str());
  return 0;
}

int handle_recv_respond(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  ReceiveOptions recv_opt;
  recv_opt.recv_channel = opts.init_recv_channel;
  recv_opt.recv_role = opts.init_recv_role;

  recv_opt.recv_address = opts.init_recv_address;
  recv_opt.send_channel = opts.init_send_channel;
  recv_opt.send_role = opts.init_send_role;
  recv_opt.alt_channel = opts.alt_channel;
  recv_opt.multi_channel = opts.multi_channel;

  // TODO: support channel role for alt channel
  // recv_opt.alt_role = opts.alt_role;
  recv_opt.timeout_ms = opts.timeout_ms;

  // TODO: this should demonstrate reacting to the received message
  auto message = readStdin();

  auto [status1, link_addr, listener] = race.receive_respond(recv_opt);
  if (status1 != ApiStatus::OK) {
    RaceLog::logError("RaceCli", "Opening listen failed\n", "");
    return 1;
  }

  printf("Listening on %s\n", link_addr.c_str());

  int packages_remaining = opts.num_packages;
  while (opts.num_packages == -1 || packages_remaining > 0) {
    auto [status2, received_message, responder] = listener.receive_str();
    if (status2 != ApiStatus::OK) {
      RaceLog::logError("RaceCli", "Receive failed\n", "");
      return 1;
    }

    printf("%s\n", received_message.c_str());

    auto status3 = responder.respond(message);
    if (status3 != ApiStatus::OK) {
      RaceLog::logError("RaceCli", "Receive failed\n", "");
      return 1;
    }

    packages_remaining--;
  }

  listener.close();

  return 0;
}

int handle_client_connect(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  if (opts.init_send_address.empty()) {
    printf("link address required\n");
    return -1;
  }

  SendOptions send_opt;
  send_opt.send_channel = opts.init_send_channel;
  send_opt.send_role = opts.init_send_role;
  send_opt.send_address =
      opts.init_send_address; // generated in handle_server_connect
  send_opt.recv_channel = opts.init_recv_channel;
  send_opt.recv_role = opts.init_recv_role;
  send_opt.alt_channel = opts.alt_channel;

  std::string introductionMsg = "hello";
  auto [status, connection] = race.dial_str(send_opt, introductionMsg);
  if (status != ApiStatus::OK) {
    printf("dial failed with status: %i\n", status);
    return -1;
  }
  printf("dial success\n");

  printf("\ntype message to send followed by <ctrl+d>\n");
  auto message = readStdin();
  std::string msgStr(message.begin(), message.end());

  int packages_remaining = opts.num_packages;
  while (opts.num_packages == -1 || packages_remaining > 0) {
    status = connection.write_str(msgStr);
    if (status != ApiStatus::OK) {
      printf("write failed with status: %i\n", status);
      break;
    } else {
      printf("wrote message: %s\n", msgStr.c_str());
    }

    auto [status2, received_message] = connection.read_str();
    if (status2 != ApiStatus::OK) {
      printf("read_str failed with status: %i\n", status2);
      status = status2;
      break;
    } else {
      printf("received message: %s\n", received_message.c_str());
    }
  }
  auto status2 = connection.close();
  if (status2 != ApiStatus::OK) {
    printf("close failed with status: %i\n", status2);
    status = status2;
  }

  return (status == ApiStatus::OK);
}

int handle_server_connect(const CmdOptions &opts) {
  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  ReceiveOptions recv_opt;
  recv_opt.recv_channel = opts.init_recv_channel;
  recv_opt.recv_role = opts.init_recv_role;
  recv_opt.send_channel = opts.init_send_channel;
  recv_opt.send_role = opts.init_send_role;

  auto [status, link_addr, listener] = race.listen(recv_opt);
  if (status != ApiStatus::OK) {
    printf("listen failed with status: %i\n", status);
    return -1;
  }

  // assume the link address is passed in out of band
  // start client with this link address
  printf("\nlistening on link address: '%s'\nbe sure to escape quotes for "
         "client\n\n",
         link_addr.c_str());

  auto [status2, connection] = listener.accept();
  if (status2 != ApiStatus::OK) {
    printf("accept failed with status: %i\n", status2);
    return -2;
  }
  printf("accept success\n");

  printf("\ntype message to send followed by <ctrl+d>\n");
  auto message = readStdin();
  std::string msgStr(message.begin(), message.end());

  auto [status5, received_message2] = connection.read_str();
  if (status5 != ApiStatus::OK) {
    printf("read failed with status: %i\n", status5);
    status = status5;
  } else {
    printf("received message: %s\n", received_message2.c_str());
  }

  int packages_remaining = opts.num_packages;
  while (opts.num_packages == -1 || packages_remaining > 0) {
    auto status3 = connection.write_str(msgStr);
    if (status3 != ApiStatus::OK) {
      printf("write failed with status: %i\n", status3);
      status = status3;
      break;
    } else {
      printf("wrote message: %s\n", msgStr.c_str());
    }

    auto [status4, received_message] = connection.read_str();
    if (status4 != ApiStatus::OK) {
      printf("read failed with status: %i\n", status4);
      status = status4;
      break;
    } else {
      printf("received message: %s\n", received_message.c_str());
    }
  }

  auto status6 = connection.close();
  if (status6 != ApiStatus::OK) {
    printf("close failed with status: %i\n", status6);
    status = status6;
  }

  return (status == ApiStatus::OK);
}


void close_socket(int &socket_fd) {
  printf("closing socket %d\n", socket_fd);
  if (-1 == ::shutdown(socket_fd, SHUT_RDWR)) { // prevent further socket IO
    printf("failed to shutdown() socket %d (%d): %s", socket_fd, errno, strerror(errno));
  }
  if (-1 == ::close(socket_fd)) {
    printf("failed to close() socket %d (%d): %s", socket_fd, errno, strerror(errno));
  }
  socket_fd = -1;
}

int create_listening_socket(int port) {
  int listening_sock, optval = 1;
  char on = 1;
  struct addrinfo hints, *res = NULL;
  char portstr[12];

  memset(&hints, 0x00, sizeof(hints));

  hints.ai_flags = AI_NUMERICSERV; // numeric service number, not resolve
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string bind_addr = "localhost";
  sprintf(portstr, "%d", port);

  // Try to resolve address if bind_address is a hostname
  if (::getaddrinfo(bind_addr.c_str(), portstr, &hints, &res) != 0) {
    printf("getadddrinfo() failed\n");
    return -1;
  }

  printf("new listening socket family:%d, type:%d, protocol:%d\n", res->ai_family, res->ai_socktype, res->ai_protocol);
  listening_sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (listening_sock >= 0 &&
      ::setsockopt(listening_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) < 0) {
    printf("setsockopt() failed\n");
    close_socket(listening_sock);
    listening_sock = -1;
  }

  // make non-blocking so ::poll() can timeout
  if (listening_sock >= 0 && ::ioctl(listening_sock, FIONBIO, &on) < 0) {
    perror("ioctl() failed to make socket non-blocking");
    close_socket(listening_sock);
    listening_sock = -1;
  }

  if (listening_sock >= 0 &&
      ::bind(listening_sock, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind() failed");
    close_socket(listening_sock);
    listening_sock = -1;
  }

  if (listening_sock >= 0 && ::listen(listening_sock, 20) < 0) {
    printf("listen() failed\n");
    close_socket(listening_sock);
    listening_sock = -1;
  }

  if (res != nullptr) {
    ::freeaddrinfo(res);
  }
  if (listening_sock > 0) {
    printf("created listening socket %d\n", listening_sock);
  }
  return listening_sock;
}

int create_client_connection(std::string &hostaddr, uint16_t port) {
  struct addrinfo hints, *res = NULL;
  int client_sock;
  char portstr[12];

  memset(&hints, 0x00, sizeof(hints));

  hints.ai_flags = AI_NUMERICSERV; // numeric service number, not resolve
  hints.ai_family = AF_UNSPEC;  // socket.h
  hints.ai_socktype = SOCK_STREAM;
  sprintf(portstr, "%d", port);

  // Try to resolve address if remote_host is a hostname
  if (::getaddrinfo(hostaddr.c_str(), portstr, &hints, &res) != 0) {
    printf("getadddrinfo() failed\n");
    return -1;
  }

  printf("SOCKET new client socket family:%d, type:%d, protocol:%d\n", res->ai_family, res->ai_socktype, res->ai_protocol);
  client_sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (client_sock >= 0 &&
      ::connect(client_sock, res->ai_addr, res->ai_addrlen) < 0) {
    printf("SOCKET connect() failed\n");
    close_socket(client_sock);
    client_sock = -1;
  }

  if (res != nullptr) {
    ::freeaddrinfo(res);
  }
  printf("SOCKET connected socket %d\n", client_sock);
  return client_sock;
}

void forward_local_to_conduit(int local_sock, Conduit &conduit) {
  local_sock = dup(local_sock);
  std::vector<uint8_t> buffer(BUF_SIZE);
  ssize_t received_bytes = ::recv(local_sock, buffer.data(), BUF_SIZE, 0);
  Raceboat::OpHandle handle = conduit.getHandle();
  printf("local_to_conduit with socket fd %d, with conduit handle %lu\n", local_sock, handle);

  while (received_bytes > 0) { // read data from input socket
    std::vector<uint8_t> result(buffer.begin(),
                                buffer.begin() + received_bytes);
    printf("Relaying data %s from local socket to conduit -- %lu\n",
      std::string(buffer.begin(), buffer.begin() + received_bytes).c_str(), conduit.getHandle());
    auto status = conduit.write(result); // send data to output socket
    if (status != ApiStatus::OK) {
      printf("conduit write failed with status: %i on socket\n", status);
      break;
    }

    if (handle != conduit.getHandle()) {
      printf("    HANDLE CHANGED!  Was %lu, now %lu\n", handle, conduit.getHandle());
    }

    received_bytes = ::recv(local_sock, buffer.data(), BUF_SIZE, 0);
  }

  if (received_bytes < 0) {
    char buf[64];
    snprintf(buf, sizeof(buf) -1, "recv() failed on fd %d", local_sock);
    perror(buf);
  } else { // 0 - indicates graceful disconnect
    printf("remote socket disconnected\n");
  }
  printf("Exiting local_to_conduit loop\n");
}

void forward_conduit_to_local(Conduit &conduit, int local_sock) {
  local_sock = dup(local_sock);
  printf("conduit_to_local with socket fd %d, with conduit handle %lu\n", local_sock, conduit.getHandle());
  ssize_t send_status;
  Raceboat::OpHandle handle = conduit.getHandle();

  while (true) {
    auto [status, buffer] = conduit.read();
    if (handle != conduit.getHandle()) {
      printf("    HANDLE CHANGED!  Was %lu, now %lu\n", handle, conduit.getHandle());
    }
    if (status != ApiStatus::OK) {
      printf("conduit read failed with status: %i\n", status);
      break;
    }
    printf("Relaying data %s from conduit to local socket\n",
      std::string(buffer.begin(), buffer.end()).c_str());
    send_status = ::send(local_sock, buffer.data(), buffer.size(), 0);
    if (send_status < 0) {
      char buf[64];
      snprintf(buf, sizeof(buf) -1, "send() failed on fd %d", local_sock);
      perror(buf);
      break;
    } else if (send_status < static_cast<ssize_t>(buffer.size())) {
      // this "shouldn't" happen, but visibility provided just in case
      printf("WARNING: sent %zd of %lu bytes\n", send_status, buffer.size());
    }
  }
  printf("Exiting conduit_to_local loop\n");
}

void relay_data_loop(int client_sock, Raceboat::Conduit &conduit, bool blocking=false) {
  printf("relay_data_loop socket: %d with conduit handle %lu\n", client_sock, conduit.getHandle());
  std::thread local_to_conduit_thread([client_sock, &conduit]() {
    forward_local_to_conduit(client_sock, conduit);
  });

  std::thread conduit_to_local_thread([client_sock, &conduit]() {
    forward_conduit_to_local(conduit, client_sock);
  });
  if (blocking) {
    local_to_conduit_thread.join();
    conduit_to_local_thread.join();
  } else {
    local_to_conduit_thread.detach();
    conduit_to_local_thread.detach();
  }
}

void client_connection_loop(int server_sock,
                            const BootstrapConnectionOptions &conn_opt,
                            Race &race) {
  pollfd poll_fd;
  memset(&poll_fd, 0, sizeof(poll_fd));
  poll_fd.fd = server_sock;
  poll_fd.events = POLLIN;
  int timeout = (5 * 60 * 1000); // 5 minute timeout
  int poll_result;

  // allow re-connect, but only 1 active connection
  do {
    poll_result = ::poll(&poll_fd, 1, timeout);
    if (poll_result < 0) {
      perror("poll() error");
    } else if (poll_result == 0) {
      printf("client loop timed out\n");
    } else if (poll_fd.revents != POLLIN) {
      printf("unexpected poll event %d.  Exiting...\n", poll_fd.revents);
    }
    
    if (poll_result > 0) {
      if (poll_result > 1) {
        // this "shouldn't" happen, but here for visibility
        printf("poll returned %d\n", poll_result);
      }
      printf("accept()ing client socket\n");
      int client_sock = ::accept(server_sock, NULL, 0);
      printf("accepted socket %d\n", client_sock);
      if (client_sock < 0) {
        perror("accept() error");
      } else {
        printf("calling bootstrap_dial_str\n");
        auto [status, connection] = race.bootstrap_dial_str(conn_opt, "");
        if (status != ApiStatus::OK) {
          printf("dial failed with status: %i\n", status);
          connection.close();
        } else {
          printf("dial success\n");
          // block so accept() isn't called until after socket error
          relay_data_loop(client_sock, connection, /* blocking */ true);
          connection.close();
          close_socket(client_sock);
        }
      }
    } else if (poll_result == 0) {
      printf("socket timeout\n");
    } else {
      perror("socket poll error");
    }
  // allow retries (via poll_result == 0)
  } while (poll_result >= 0);

  printf("exiting client loop\n");
}

void check_for_local_port_override(const CmdOptions &opts, int &local_port) {
  for (auto pair: opts.params) {
    if (pair.first == "localPort") {
      local_port = stoi(pair.second);
      printf("local port: %d\n", local_port);
      break;
    }
  }
}

int handle_client_bootstrap_connect(const CmdOptions &opts) {
  // listen for localhost connection
  // dial into / connect to race connection
  // connect to race conduit connections
  // relay data to race conduit side
  // relay data from race conduit to localhost

  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  if (opts.init_send_address.empty()) {
    printf("link address required\n");
    return -1;
  }

  BootstrapConnectionOptions conn_opt;
  conn_opt.init_send_channel = opts.init_send_channel;
  conn_opt.init_send_role = opts.init_send_role;
  conn_opt.init_send_address =
      opts.init_send_address; // generated in handle_server_connect
  conn_opt.init_recv_channel = opts.init_recv_channel;
  conn_opt.init_recv_role = opts.init_recv_role;
  conn_opt.final_send_channel = opts.final_send_channel;
  conn_opt.final_send_role = opts.final_send_role;
  conn_opt.final_recv_channel = opts.final_recv_channel;
  conn_opt.final_recv_role = opts.final_recv_role;

  int local_port = 9999;
  check_for_local_port_override(opts, local_port);

  int server_sock;
  printf("CREATING LOCAL SOCKET\n");
  // start server for client app to connect to
  if ((server_sock = create_listening_socket(local_port)) < 0) {
    printf("Failed to create local socket\n");
    return -1;
  }

  client_connection_loop(server_sock, conn_opt, race);

  printf("closing local socket\n");
  close_socket(server_sock);

  return 0;
}

ApiStatus server_connections_loop(Race &race, BootstrapConnectionOptions &conn_opt, int local_port) {
  ApiStatus status = ApiStatus::OK;
  
  printf("CREATING RACE SERVER SOCKET\n");
  // listen on race side
  auto [status1, link_addr, listener] = race.bootstrap_listen(conn_opt);
  if (status1 != ApiStatus::OK) {
    printf("listen failed with status: %i\n", status1);
    return status1;
  }
  printf("\nlistening on link address: '%s'", link_addr.c_str());

  std::string host = "localhost";
  std::unordered_map<OpHandle, Raceboat::Conduit> connections;
  while (1) {
    printf("server calling accept\n");
    auto [status2, connection] = listener.accept();
    if (status2 != ApiStatus::OK) {
      printf("accept failed with status: %i\n", status2);
      status = status2;
      break;
    }
  
    
    printf("AWAITING LOCAL CLIENT\n");
    
    // create client connection for listening socket to connect on
    // assume listening process is or will be running
    int client_sock = -1;
    while (client_sock < 0) {
      if ((client_sock = create_client_connection(host, local_port)) < 0) {
        printf("Awaiting listening socket \n");
        sleep(5);
      }
    }

    printf("accept success\n");
    connections[connection.getHandle()] = connection;
    printf("SOCKET client_sock: %d\n", client_sock);
    relay_data_loop(client_sock, connections[connection.getHandle()]);

    for (auto handleConnPair: connections) {
      void* ptr = &handleConnPair.second;
      printf(" -- %lu:%lu - %p\n", handleConnPair.first, handleConnPair.second.getHandle(), ptr);
      if (handleConnPair.first != handleConnPair.second.getHandle()) {
        printf("CONDUIT HANDLE CHANGED! Was %lu, now %lu\n", handleConnPair.first, handleConnPair.second.getHandle());
      }
    }
  }

  printf("closing race sockets\n");
  for (auto conn: connections) {
    auto close_status = conn.second.close();
    if (ApiStatus::OK != close_status) {
      printf("close failed with status: %i\n", close_status);
    }
  }
  return status;
}

int handle_server_bootstrap_connect(const CmdOptions &opts) {
  // connect to localhost
  // listen for race connections
  // relay data from race conduit to localhost
  // relay data from localhost to race conduit

  ChannelParamStore params = getParams(opts);

  Race race(opts.plugin_path, params);

  BootstrapConnectionOptions conn_opt;
  conn_opt.init_recv_channel = opts.init_recv_channel;
  conn_opt.init_recv_role = opts.init_recv_role;
  conn_opt.init_recv_address = opts.init_recv_address;
  conn_opt.init_send_channel = opts.init_send_channel;
  conn_opt.init_send_role = opts.init_send_role;
  conn_opt.init_send_address = opts.init_send_address;
  conn_opt.final_recv_channel = opts.final_recv_channel;
  conn_opt.final_recv_role = opts.final_recv_role;
  conn_opt.final_send_channel = opts.final_send_channel;
  conn_opt.final_send_role = opts.final_send_role;
  
  printf("handle_server_bootstrap_connect\n");
  
  int local_port = 7777;
  check_for_local_port_override(opts, local_port);
  ApiStatus status = server_connections_loop(race, conn_opt, local_port);

  return (status == ApiStatus::OK);
}

int main(int argc, char **argv) {
  auto opts = parseOpts(argc, argv);
  if (!opts.has_value()) {
    return 1;
  }

  RaceLog::setLogLevel(opts->log_level);

  int result = -1;

  switch (opts->mode) {
  case Mode::SEND_ONESHOT:
    result = handle_send_oneshot(*opts);
    break;
  case Mode::SEND_RECV:
    result = handle_send_recv(*opts);
    break;
  case Mode::CLIENT_CONNECT:
    result = handle_client_connect(*opts);
    break;
  case Mode::RECV_RESPOND:
    result = handle_recv_respond(*opts);
    break;
  case Mode::RECV_ONESHOT:
    result = handle_recv_oneshot(*opts);
    break;
  case Mode::SERVER_CONNECT:
    result = handle_server_connect(*opts);
    break;
  case Mode::SERVER_BOOTSTRAP_CONNECT:
    result = handle_server_bootstrap_connect(*opts);
    break;
  case Mode::CLIENT_BOOTSTRAP_CONNECT:
    result = handle_client_bootstrap_connect(*opts);
    break;
  default:
    printf("%s: A mode must be selected [send, send-recv, client-connect, "
           "recv, recv-reply "
           "server-connect]\n",
           argv[0]);
  }

  return result;
}
