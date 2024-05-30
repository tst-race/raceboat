
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
#include <fstream>
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
  std::string passphrase = "";
  std::string responses_file = "";

  ChannelId init_recv_channel;
  std::string init_recv_role = "default";
  ChannelId init_send_channel;
  std::string init_send_role = "default";
  LinkAddress init_send_address;
  LinkAddress init_recv_address;
  bool multi_channel = false;
};

static std::optional<CmdOptions> parseOpts(int argc, char **argv) {
  static int log_level = RaceLog::LL_INFO;
  static int mode = static_cast<int>(Mode::INVALID);

  static option long_options[] = {
      // logging
      {"debug", no_argument, &log_level, RaceLog::LogLevel::LL_DEBUG},
      {"quiet", no_argument, &log_level, RaceLog::LogLevel::LL_ERROR},

      // channel selection
      {"recv-channel", required_argument, nullptr, 'R'},
      {"recv-role", required_argument, nullptr, 'r'},
      {"send-channel", required_argument, nullptr, 'S'},
      {"send-role", required_argument, nullptr, 's'},

      // destination selection
      {"send-address", required_argument, nullptr, 'a'},
      {"recv-address", required_argument, nullptr, 'e'},

      // race plugin path
      {"dir", required_argument, nullptr, 'd'},

      // bridge address list file
      {"responses-file", required_argument, nullptr, 'f'},

      // Authentication phrase
      {"passphrase", required_argument, nullptr, 'c'},

      // param
      {"param", required_argument, nullptr, 'p'},

      // allow send and recv on multiple channels
      {"multi-channel", no_argument, nullptr, 'm'},

      // help
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  int c;
  CmdOptions opts;

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "R:r:S:s:T:t:a:e:p:c:f:mdhwn:", long_options,
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

    case 'a':
      opts.init_send_address = optarg;
      break;

    case 'e':
      opts.init_recv_address = optarg;
      break;

    case 'd':
      opts.plugin_path = optarg;
      break;

    case 'c':
      opts.passphrase = optarg;
      break;

    case 'f':
      opts.responses_file = optarg;
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
                    "Channel Selection:\n"
                    "    --recv-channel, -R  Set the channel to receive with\n"
                    "    --send-channel, -S  Set the channel to send with\n"
                    "    --send-address, -a  Set the address to send to\n"
                    "    --recv-address, -e  Set the address to listen to (optional, creates a new address by default)\n"
                    "\n"
                    "Channel Parameters:\n"
                    "    --param, -p         Parameters used to specify information necessary for channels to function, e.g. hostname, or account credentials\n"
                    "\n"
                    "Misc:\n"
                    "    --responses-file, -f           The file to read responses from\n"
                    "    --passphrase, -a           The phrase clients are expected to authenticate with\n"
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

std::vector<std::vector<uint8_t>> readBridges(std::string bridgeFilepath) {
  std::vector<std::vector<uint8_t>> lines;

  std::ifstream file_in(bridgeFilepath.c_str());
  if (!file_in) {
    printf("ERROR: cannot find %s\n", bridgeFilepath.c_str());
    return {};
  }

  std::string line;
  while (std::getline(file_in, line)) {
    lines.emplace_back(std::vector<uint8_t>(line.begin(), line.end()));
  }

  return lines;
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
  recv_opt.multi_channel = opts.multi_channel;

  auto responses = readBridges(opts.responses_file);
  auto passphrase = opts.passphrase;

  auto [status1, link_addr, listener] = race.receive_respond(recv_opt);
  if (status1 != ApiStatus::OK) {
    RaceLog::logError("RaceCli", "Opening listen failed\n", "");
    return 1;
  }

  printf("Listening on %s\n", link_addr.c_str());

  size_t resp_idx = 0;
  while (true) {
    auto [status2, received_message, responder] = listener.receive_str();
    if (status2 != ApiStatus::OK) {
      RaceLog::logError("RaceCli", "Receive failed\n", "");
      return 1;
    }
    
    printf("RECEIVED REQUEST: %s\n", received_message.c_str());
    if (received_message == passphrase) {
      auto response = responses[resp_idx];
      resp_idx = (resp_idx + 1) % responses.size();
      auto status3 = responder.respond(response);
      if (status3 != ApiStatus::OK) {
        RaceLog::logError("RaceCli", "Response failed\n", "");
        return 1;
      }
    }
    else {
      printf("Client did not authenticate");
    }
  }

  listener.close();

  return 0;
}


int main(int argc, char **argv) {
  auto opts = parseOpts(argc, argv);
  if (!opts.has_value()) {
    return 1;
  }

  RaceLog::setLogLevel(opts->log_level);

  int result = -1;
  result = handle_recv_respond(*opts);

  return result;
}
