
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
#include <utility>
#include <vector>

#include "race/Race.h"
#include "race/common/RaceLog.h"

// #include <boost/asio.hpp>
// #include <boost/bind.hpp>
// #include <boost/shared_ptr.hpp>
// #include <boost/enable_shared_from_this.hpp>


#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#define READ  0
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

/* Forward data between sockets */
void forward_local_to_conduit(int local_sock, Conduit conduit) {
  printf("In local_to_conduit\n");
    ssize_t n;

    std::vector<uint8_t> buffer(BUF_SIZE);

    while ((n = recv(local_sock, buffer.data(), BUF_SIZE, 0)) > 0) { // read data from input socket
      printf("Writing to conduit\n");
       auto status = conduit.write(buffer); // send data to output socket
      if (status != ApiStatus::OK) {
        printf("write failed with status: %i\n", status);
        break;
      }
    }
    printf("Exited local_to_conduit loop\n");

    if (n < 0) {
        printf("read error");
        exit(1);
    }

    conduit.close();

    shutdown(local_sock, SHUT_RDWR); // stop other processes from using socket
    close(local_sock);
}

void forward_conduit_to_local(Conduit conduit, int local_sock) {
  printf("In conduit_to_local\n");
    while (true) {
      auto [status, buffer] = conduit.read();
      if (status != ApiStatus::OK) {
        printf("read failed with status: %i\n", status);
        break;
      }
      printf("Writing to local socket\n");
      send(local_sock, buffer.data(), buffer.size(), 0);
    }
    printf("Exited conduit_to_local loop\n");

    conduit.close();

    shutdown(local_sock, SHUT_RDWR); // stop other processes from using socket
    close(local_sock);
}

/* Create server socket */
int create_socket(int port) {
    int local_sock, optval = 1;
    // int validfamily=0;
    struct addrinfo hints, *res=NULL;
    char portstr[12];

    memset(&hints, 0x00, sizeof(hints));

    hints.ai_flags    = AI_NUMERICSERV;   /* numeric service number, not resolve */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    /* prepare to bind on specified numeric address */
    // std::string bind_addr;
    // if (!bind_addr.empty()) {
    //     /* check for numeric IP to specify IPv6 or IPv4 socket */
    //     if (validfamily = check_ipversion(bind_addr)) {
    //          hints.ai_family = validfamily;
    //          hints.ai_flags |= AI_NUMERICHOST; /* bind_addr is a valid numeric ip, skip resolve */
    //     }
    // } else {
    //     /* if bind_address is NULL, will bind to IPv6 wildcard */
    //     hints.ai_family = AF_INET6; /* Specify IPv6 socket, also allow ipv4 clients */
    //     hints.ai_flags |= AI_PASSIVE; /* Wildcard address */
    // }
    std::string bind_addr = "localhost";

    sprintf(portstr, "%d", port);

    /* Check if specified socket is valid. Try to resolve address if bind_address is a hostname */
    if (getaddrinfo(bind_addr.c_str(), portstr, &hints, &res) != 0) {
        return -1;
    }

    if ((local_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        return -1;
    }


    if (setsockopt(local_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return -1;
    }

    if (bind(local_sock, res->ai_addr, res->ai_addrlen) == -1) {
            close(local_sock);
        return -1;
    }

    if (listen(local_sock, 20) < 0) {
        return -1;
    }

    if (res != NULL) {
        freeaddrinfo(res);
    }

    return local_sock;
}

// /* Create client connection */
// int create_connection() {
//     struct addrinfo hints, *res=NULL;
//     int sock;
//     int validfamily=0;
//     char portstr[12];

//     memset(&hints, 0x00, sizeof(hints));

//     hints.ai_flags    = AI_NUMERICSERV; /* numeric service number, not resolve */
//     hints.ai_family   = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;

//     sprintf(portstr, "%d", remote_port);

//     /* check for numeric IP to specify IPv6 or IPv4 socket */
//     if (validfamily = check_ipversion(remote_host)) {
//          hints.ai_family = validfamily;
//          hints.ai_flags |= AI_NUMERICHOST;  /* remote_host is a valid numeric ip, skip resolve */
//     }

//     /* Check if specified host is valid. Try to resolve address if remote_host is a hostname */
//     if (getaddrinfo(remote_host,portstr , &hints, &res) != 0) {
//         errno = EFAULT;
//         return CLIENT_RESOLVE_ERROR;
//     }

//     if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
//         return CLIENT_SOCKET_ERROR;
//     }

//     if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
//         return CLIENT_CONNECT_ERROR;
//     }

//     if (res != NULL) {
//         freeaddrinfo(res);
//     }

//     return sock;
// }

void handle_client(int client_sock,
                   const BootstrapConnectionOptions &conn_opt,
                   Race &race,
                   const std::string &introductionMsg)
{

  printf("calling bootstrap_dial_str\n");
  auto [status, connection] = race.bootstrap_dial_str(conn_opt, introductionMsg);
  if (status != ApiStatus::OK) {
    printf("dial failed with status: %i\n", status);
    return;
  }
  printf("dial success\n");
  
    // if ((remote_sock = create_connection()) < 0) {
    //     printf("Cannot connect to host");
    //     goto cleanup;
    // }

  // printf("forking for local_to_conduct");
  //   if (fork() == 0) { // a process forwarding data from client to remote socket
  //     forward_conduit_to_local(connection, client_sock);
  //     // forward_local_to_conduit(client_sock, connection);
  //     exit(0);
  //   }

  printf("forking for conduit_to_local");
    // if (fork() == 0) { // a process forwarding data from remote socket to client
      forward_local_to_conduit(client_sock, connection);
      // forward_conduit_to_local(connection, client_sock);
      // exit(0);
    // }

    // connection.close();
    // close(client_sock);
}

void client_connection_loop(int local_sock,
                            const BootstrapConnectionOptions &conn_opt,
                            Race &race,
                            const std::string &introductionMsg) {
    // struct sockaddr_storage client_addr;
    // socklen_t addrlen = sizeof(client_addr);
  int client_sock;
    // int client_sock = local_sock;

    // handle_client(client_sock, conn_opt, race, introductionMsg);
            
    while (true) {
        // update_connection_count();
      client_sock = accept(local_sock, NULL, 0);
       // if (fork() == 0) { // handle client connection in a separate process
      close(local_sock);
      handle_client(client_sock, conn_opt, race, introductionMsg);
        //     exit(0);
        // }
        // if (fork() == 0) { // handle client connection in a separate process
        //     close(local_sock);
        //     handle_client(client_sock, conn_opt, race, introductionMsg);
        //     exit(0);
        // }
        // close(client_sock);
    }

}

int handle_client_bootstrap_connect(const CmdOptions &opts) {
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

  std::string introductionMsg = "hello";

  // auto [status, connection] = race.bootstrap_dial_str(conn_opt, introductionMsg);
  // if (status != ApiStatus::OK) {
  //   printf("dial failed with status: %i\n", status);
  // }
  // auto message = readStdin();
  // std::string msgStr(message.begin(), message.end());
  // // int packages_remaining = opts.num_packages;
  // status = connection.write_str(msgStr);
  // while (opts.num_packages == -1 || packages_remaining > 0) {
  //   status = connection.write_str(msgStr);
  //   if (status != ApiStatus::OK) {
  //     printf("write failed with status: %i\n", status);
  //     break;
  //   } else {
  //     printf("wrote message: %s\n", msgStr.c_str());
  //   }
  // }
 
  int local_port = 9999;
  int local_sock;
  printf("CREATING LOCAL SOCKET\n");
  if ((local_sock = create_socket(local_port)) < 0) { // start server
    printf("Failed to create local socket\n");
    return -1;
  }

  //   // signal(SIGCHLD, sigchld_handler); // prevent ended children from becoming zombies
  // // signal(SIGTERM, sigterm_handler); // handle KILL signal
  client_connection_loop(local_sock, conn_opt, race, introductionMsg);


  // auto status2 = connection.close();
  // if (status2 != ApiStatus::OK) {
  //   printf("close failed with status: %i\n", status2);
  //   status = status2;
  // }

  return 0;
}

int handle_server_bootstrap_connect(const CmdOptions &opts) {
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

  auto [status, link_addr, listener] = race.bootstrap_listen(conn_opt);
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

  auto [status5, received_message2] = connection.read_str();
  if (status5 != ApiStatus::OK) {
    printf("read failed with status: %i\n", status5);
    status = status5;
  } else {
    printf("received message: %s\n", received_message2.c_str());
  }

  printf("\ntype message to send followed by <ctrl+d>\n");
  int packages_remaining = opts.num_packages;
  while (opts.num_packages == -1 || packages_remaining > 0) {
    // auto message = readStdin();
    // std::string msgStr(message.begin(), message.end());
    // if (msgStr == "quit") {
    //   break;
    // }

    // if (!msgStr.empty()) {
    //   auto status3 = connection.write_str(msgStr);
    //   if (status3 != ApiStatus::OK) {
    //     printf("write failed with status: %i\n", status3);
    //     status = status3;
    //     break;
    //   } else {
    //     printf("wrote message: %s\n", msgStr.c_str());
    //   }
    // }

    auto [status4, received_message] = connection.read_str();
    if (status4 != ApiStatus::OK) {
      printf("read failed with status: %i\n", status4);
      status = status4;
      break;
    } else {
      if (!received_message.empty()) {
        printf("received message: %s\n", received_message.c_str());
      }
    }
    packages_remaining--;
  }

  auto status6 = connection.close();
  if (status6 != ApiStatus::OK) {
    printf("close failed with status: %i\n", status6);
    status = status6;
  }

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
