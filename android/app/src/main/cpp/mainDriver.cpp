
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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
//cstdlib may not be needed
#include "race/Race.h"
#include "race/common/RaceLog.h"
#include <android/log.h>
#include <jni.h>
#include <nlohmann/json.hpp>

using namespace Raceboat;

enum class Mode : int {
    INVALID,
    SEND_ONESHOT,
    SEND_RECV,
    CLIENT_CONNECT,
    RECV_ONESHOT,
    RECV_RESPOND,
    SERVER_CONNECT,
};

// Function to convert string to Mode enum
Mode stringToMode(const std::string& modeStr) {
    static const std::map<std::string, Mode> modeMap = {
            {"INVALID", Mode::INVALID},
            {"Send Once", Mode::SEND_ONESHOT},
            {"Request-Reply", Mode::SEND_RECV},
            {"Connection", Mode::CLIENT_CONNECT},
//            {"RECV_ONESHOT", Mode::RECV_ONESHOT},
//            {"RECV_RESPOND", Mode::RECV_RESPOND},
//            {"SERVER_CONNECT", Mode::SERVER_CONNECT}
    };

    auto it = modeMap.find(modeStr);
    if (it != modeMap.end()) {
        return it->second;
    } else {
        // Handle unknown mode string
        return Mode::INVALID; // Or any wanted error handling if necessary
    }
}

// Function to get the application's data directory
std::string getAppDataDir() {
    char* appDataDir = getenv("ANDROID_DATA");
    if (appDataDir != nullptr) {
        return std::string(appDataDir) + "/data/com.twosixtech.raceboat/";
    } else {
        return "/data/data/com.twosixtech.raceboat/";
    }
}

//these functions use std::cerr and std::cout to try to make android log/debugging easier
std::vector<uint8_t> readSocketData(int socket) {
    std::uint32_t length;
    if (recv(socket, &length, sizeof(length), 0) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "readSocketData", "failed to read length bytes");
    }

    __android_log_print(ANDROID_LOG_DEBUG, "readSocketData", "read length %d", length);

    std::vector<uint8_t> buffer(length);
    if (recv(socket, buffer.data(), length, 0) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "readSocketData", "failed to read length bytes");
    }

    __android_log_print(ANDROID_LOG_DEBUG, "readSocketData", "read data %d", buffer.size());

    return buffer;
}

void sendMessageToJavaClient(int clientSocket, const std::string& message) {
    ssize_t bytesSent = send(clientSocket, message.c_str(), message.length(), 0);
    if (bytesSent == -1) {
        std::cerr << "Error sending data to socket\n";
    }
}

std::string receiveMessageFromJavaClient(int clientSocket) {
    std::vector<uint8_t> receivedData = readSocketData(clientSocket);
    return std::string(receivedData.begin(), receivedData.end());
}

int createServerSocket() {
    __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "createServerSocket");
    std::string socketPath = getAppDataDir() + "raceboat_socket.sock";
    socketPath = "RaceboatLocalSocket";
    struct sockaddr_un server_address;

    int serverSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        __android_log_print(ANDROID_LOG_ERROR, "MainDriver", "error creating socket %d", serverSocket);
        std::cerr << "Error creating server socket\n";
        return -1;

    }
    memset(&server_address, 0, sizeof(server_address));
    server_address.sun_family = AF_LOCAL;
    strncpy(server_address.sun_path + 1, socketPath.c_str(), sizeof(server_address.sun_path) - 2);
    int size = sizeof(server_address) - sizeof(server_address.sun_path) + strlen(server_address.sun_path+1) +1;

    __android_log_print(ANDROID_LOG_DEBUG, "createServerSocket", "address.sun_path %d %s %d",
                        sizeof(server_address.sun_path),
                        &server_address.sun_path[1],
                        server_address.sun_path[0]);

    if (bind(serverSocket,
             (struct sockaddr*)&server_address,
             size) < 0) {
        std::cerr << "Error binding server socket\n";
        __android_log_print(ANDROID_LOG_ERROR, "createServerSocket", "error binding socket");
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Error listening on server socket\n";
        close(serverSocket);
        return -1;
    }
    __android_log_print(ANDROID_LOG_ERROR, "createServerSocket", "LISTENING");

    return serverSocket;
}

int acceptClientConnection(int serverSocket) {
    struct sockaddr_un clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    __android_log_print(ANDROID_LOG_ERROR,
                        "acceptClientConnection", "attempting to accept");
    int clientSocket = accept(serverSocket,
                              (struct sockaddr*)&clientAddress,
                              &clientAddressLength);
    if (clientSocket == -1) {
        __android_log_print(ANDROID_LOG_ERROR,
                            "acceptClientConnection", "error accepting client");
        std::cerr << "Error accepting client connection\n";
    }
    return clientSocket;
}

//may need to adjust the options to store the necessary socket data that provides the cpp - java functionality
struct RaceboatOptions {
    Mode mode;
    RaceLog::LogLevel log_level;
    std::vector<std::pair<std::string, std::string>> params;

    std::string plugin_path = "/data/data/com.example.raceboat/files/";

    ChannelId recv_channel;
    std::string recv_role = "default";
    ChannelId send_channel;
    std::string send_role = "default";
    ChannelId alt_channel;
    std::string alt_role = "default";
    LinkAddress send_address;
    LinkAddress recv_address;
    int timeout_ms = 0;
    bool multi_channel = false;

    int num_packages = -1;
};

//can possibly hardcode RaceBoatOptions
//or create hardcoded object(s) that can be called by main or any other func to make testing easier
static std::optional<RaceboatOptions> parseOptsFromFile(std::string configFilepath){
    return {};
}

static std::optional<RaceboatOptions> parseOpts(){
    RaceboatOptions opts;
    //opts.send_channel is a ChannelId, hoping this handles string to ChannelId for me
    opts.send_channel = "twoSixDirectCpp";
    opts.recv_channel = "twoSixDirectCpp";
    //set the mode
    opts.mode = Mode::SEND_ONESHOT;
    //opts.mode = Mode::SEND_RECV;
    //set the send address
    opts.send_address = "{\"hostname\":\"10.0.2.2\",\"port\":26262}";
    //set the receive address
    //opts.recv_address =

    //how to hardcode params in
    //opts.params.push_back(std::make_pair("key1", "value1"));
    opts.params.push_back(std::make_pair("hostname", "127.0.0.1"));
    opts.params.push_back(std::make_pair("PluginCommsTwoSixStub.startPort", "26262"));
    opts.params.push_back(std::make_pair("PluginCommsTwoSixStub.endPort", "26264"));

    //keep these around for later
    //opts.mode = static_cast<Mode>(mode);
    //opts.log_level = static_cast<RaceLog::LogLevel>(log_level);
    return {opts};
}

//take channel options and move them into a channel parameter key-val map
ChannelParamStore getParams(const RaceboatOptions &opts) {
    ChannelParamStore params;

    for (auto [key, value] : opts.params) {
        RaceLog::logDebug("RaceBoat",
                          "Got parameter: '" + key + "' = '" + value + "'", "");
        params.setChannelParam(key, value);
    }
    return params;
}

int handle_send_oneshot(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    RaceLog::logDebug("RaceBoat", "RUNNING handle_send_oneshot", "");
    RaceLog::logDebug("RaceBoat", "plugin_path: " + opts.plugin_path, "");

    Race race(opts.plugin_path, params);

    SendOptions send_opt;
    send_opt.send_channel = opts.send_channel;
    send_opt.send_role = opts.send_role;
    send_opt.send_address = opts.send_address;
    send_opt.recv_channel = opts.recv_channel;
    send_opt.recv_role = opts.recv_role;
    send_opt.alt_channel = opts.alt_channel;

    // TODO: support channel role for alt channel
    // send_opt.alt_role = opts.alt_role;
    send_opt.timeout_ms = opts.timeout_ms;

    //can either make a new func to replace readStdin() i.e. read the socket wrapper
    //or hardcode a message/check hardcoded objects
    //initially, message needs to be a hardcoded vector<uint8_t>
    //i.e. a vector of chars
    //std::string testMessage = "This is a test message";
    //std::vector<uint8_t> testVec(testMessage.begin(), testMessage.end());

    //auto message = readStdin();
    //auto message = testVec;
    std::string messageFromJavaClient = receiveMessageFromJavaClient(javaClientSocket);
    std::vector<uint8_t> clientMessageVec(messageFromJavaClient.begin(), messageFromJavaClient.end());
    auto message = clientMessageVec;

    auto status = race.send(send_opt, message);
    if (status != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "Send failed", "");
        return 1;
    }

    return 0;
}

int handle_recv_oneshot(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    Race race(opts.plugin_path, params);

    ReceiveOptions recv_opt;
    recv_opt.recv_channel = opts.recv_channel;
    recv_opt.recv_role = opts.recv_role;

    recv_opt.recv_address = opts.recv_address;
    recv_opt.send_channel = opts.send_channel;
    recv_opt.send_role = opts.send_role;
    recv_opt.alt_channel = opts.alt_channel;
    recv_opt.multi_channel = opts.multi_channel;

    // TODO: support channel role for alt channel
    // recv_opt.alt_role = opts.alt_role;
    recv_opt.timeout_ms = opts.timeout_ms;

    auto [status1, link_addr, listener] = race.receive(recv_opt);
    if (status1 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "Opening listen failed\n", "");
        return 1;
    }

    RaceLog::logInfo("RaceBoat", "Listening on " +  std::string(link_addr.c_str()) + "\n", "");

    int packages_remaining = opts.num_packages;
    while (opts.num_packages == -1 || packages_remaining > 0) {
        auto [status2, received_message] = listener.receive_str();
        if (status2 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "Receive failed\n", "");
            return 1;
        }

        RaceLog::logInfo("RaceBoat", std::string(received_message.c_str()) + "\n", "");

        //this may need to be outside of this while loop
        sendMessageToJavaClient(javaClientSocket, received_message);

        packages_remaining--;
    }
    //may need to build message in while loop then send full thing to java client here
    //sendMessageToJavaClient(javaClientSocket, received_message);
    listener.close();

    return 0;
}

int handle_send_recv(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    Race race(opts.plugin_path, params);

    SendOptions send_opt;
    send_opt.send_channel = opts.send_channel;
    send_opt.send_role = opts.send_role;
    send_opt.send_address = opts.send_address;
    send_opt.recv_channel = opts.recv_channel;
    send_opt.recv_role = opts.recv_role;
    send_opt.alt_channel = opts.alt_channel;

    // TODO: support channel role for alt channel
    // send_opt.alt_role = opts.alt_role;
    send_opt.timeout_ms = opts.timeout_ms;

    //message needs to be a hardcoded vector<uint8_t>
    //i.e. a vector of chars
    //std::string testMessage = "This is a test message";
    //std::vector<uint8_t> testVec(testMessage.begin(), testMessage.end());

    //auto message = readStdin();
    //auto message = testVec;
    std::string messageFromJavaClient = receiveMessageFromJavaClient(javaClientSocket);
    std::vector<uint8_t> clientMessageVec(messageFromJavaClient.begin(), messageFromJavaClient.end());
    auto message = clientMessageVec;

    auto [status, received_message] =
            race.send_receive_str(send_opt, {message.begin(), message.end()});
    if (status != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "Send/Receive failed", "");
        return 1;
    }

    RaceLog::logInfo("RaceBoat", std::string(received_message.c_str()) + "\n", "");
    sendMessageToJavaClient(javaClientSocket, received_message);
    return 0;
}

int handle_recv_respond(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    Race race(opts.plugin_path, params);

    ReceiveOptions recv_opt;
    recv_opt.recv_channel = opts.recv_channel;
    recv_opt.recv_role = opts.recv_role;

    recv_opt.recv_address = opts.recv_address;
    recv_opt.send_channel = opts.send_channel;
    recv_opt.send_role = opts.send_role;
    recv_opt.alt_channel = opts.alt_channel;
    recv_opt.multi_channel = opts.multi_channel;

    // TODO: support channel role for alt channel
    // recv_opt.alt_role = opts.alt_role;
    recv_opt.timeout_ms = opts.timeout_ms;

    // TODO: this should demonstrate reacting to the received message
    //message needs to be a hardcoded vector<uint8_t>
    //i.e. a vector of chars
    //std::string testMessage = "This is a test message";
    //std::vector<uint8_t> testVec(testMessage.begin(), testMessage.end());

    //auto message = readStdin();
    //auto message = testVec;
    std::string messageFromJavaClient = receiveMessageFromJavaClient(javaClientSocket);
    std::vector<uint8_t> clientMessageVec(messageFromJavaClient.begin(), messageFromJavaClient.end());
    auto message = clientMessageVec;

    auto [status1, link_addr, listener] = race.receive_respond(recv_opt);
    if (status1 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "Opening listen failed\n", "");
        return 1;
    }

    RaceLog::logInfo("RaceBoat", "Listening on " + std::string(link_addr.c_str()) + "\n", "");

    int packages_remaining = opts.num_packages;
    while (opts.num_packages == -1 || packages_remaining > 0) {
        auto [status2, received_message, responder] = listener.receive_str();
        if (status2 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "Receive failed\n", "");
            return 1;
        }

        RaceLog::logInfo("RaceBoat", std::string(received_message.c_str()) + "\n", "");

        //this may need to be outside of the while loop
        sendMessageToJavaClient(javaClientSocket, received_message);

        auto status3 = responder.respond(message);
        if (status3 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "Receive failed\n", "");
            return 1;
        }

        packages_remaining--;
    }
    //may need to build message in while loop then send full thing to java client here
    //sendMessageToJavaClient(javaClientSocket, received_message);
    listener.close();

    return 0;
}

// Map enum values to string representations
const std::unordered_map<ApiStatus, std::string> apiStatusToStringMap = {
        {ApiStatus::INVALID, "INVALID"},
        {ApiStatus::OK, "OK"},
        {ApiStatus::CLOSING, "CLOSING"},
        {ApiStatus::CHANNEL_INVALID, "CHANNEL_INVALID"},
        {ApiStatus::INVALID_ARGUMENT, "INVALID_ARGUMENT"},
        {ApiStatus::PLUGIN_ERROR, "PLUGIN_ERROR"},
        {ApiStatus::INTERNAL_ERROR, "INTERNAL_ERROR"}
};


//adding/removing this function adds/removes the duplicate function error when building
// Function to convert ApiStatus enum to string
std::string getApiStatusString(ApiStatus status) {
    auto it = apiStatusToStringMap.find(status);
    if (it != apiStatusToStringMap.end()) {
        return it->second;
    } else {
        return "UNKNOWN_STATUS";
    }
}

int handle_client_connect(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    Race race(opts.plugin_path, params);

    if (opts.send_address.empty()) {
        RaceLog::logError("RaceBoat", "link address required\n", "");
        return -1;
    }

    SendOptions send_opt;
    send_opt.send_channel = opts.send_channel;
    send_opt.send_role = opts.send_role;
    send_opt.send_address =opts.send_address; // generated in handle_server_connect
    send_opt.recv_channel = opts.recv_channel;
    send_opt.recv_role = opts.recv_role;
    send_opt.alt_channel = opts.alt_channel;

    std::string introductionMsg = "hello";

    auto [status, connection] = race.dial_str(send_opt, introductionMsg);
    if (status != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "dial failed with status: " + getApiStatusString(status) + "\n", "");
        return -1;
    }

    RaceLog::logInfo("RaceBoat", "dial success\n", "");

    //printf("\ntype message to send followed by <ctrl+d>\n");

    //message needs to be a hardcoded vector<uint8_t>
    //i.e. a vector of chars
    //std::string testMessage = "This is a test message";
    //std::vector<uint8_t> testVec(testMessage.begin(), testMessage.end());

    //auto message = readStdin();
    //auto message = testVec;
    std::string messageFromJavaClient = receiveMessageFromJavaClient(javaClientSocket);
    std::vector<uint8_t> clientMessageVec(messageFromJavaClient.begin(), messageFromJavaClient.end());
    auto message = clientMessageVec;
    std::string msgStr(message.begin(), message.end());

    int packages_remaining = opts.num_packages;
    while (opts.num_packages == -1 || packages_remaining > 0) {
        status = connection.write_str(msgStr);
        if (status != ApiStatus::OK) {
            //this could maybe be logWarning or logInfo
            RaceLog::logError("RaceBoat", "write failed with status: " + getApiStatusString(status) + "\n", "");
            break;
        } else {
            RaceLog::logInfo("RaceBoat", "wrote message: " + std::string(msgStr.c_str()) + "\n", "");
        }

        auto [status2, received_message] = connection.read_str();
        if (status2 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "read_str failed with status: " + getApiStatusString(status2) + "\n", "");
            status = status2;
            break;
        } else {
            RaceLog::logInfo("RaceBoat", "received message: " + std::string(received_message.c_str()) + "\n", "");
            sendMessageToJavaClient(javaClientSocket, received_message);
        }
    }
    auto status2 = connection.close();
    if (status2 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "close failed with status: " + getApiStatusString(status2) + "\n", "");
        status = status2;
    }

    return (status == ApiStatus::OK);
}

int handle_server_connect(const RaceboatOptions &opts, int javaClientSocket) {
    ChannelParamStore params = getParams(opts);

    Race race(opts.plugin_path, params);

    ReceiveOptions recv_opt;
    recv_opt.recv_channel = opts.recv_channel;
    recv_opt.recv_role = opts.recv_role;
    recv_opt.send_channel = opts.send_channel;
    recv_opt.send_role = opts.send_role;

    auto [status, link_addr, listener] = race.listen(recv_opt);
    if (status != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "listen failed with status: " + getApiStatusString(status) + "\n", "");
        return -1;
    }

    // assume the link address is passed in out of band
    // start client with this link address
    //printf("\nlistening on link address: '%s'\nbe sure to escape quotes for "
    //       "client\n\n",
    //       link_addr.c_str());
    RaceLog::logInfo("RaceBoat", "listening on link address: " + std::string(link_addr.c_str()) + "\n", "");

    auto [status2, connection] = listener.accept();
    if (status2 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "accept failed with status: " + getApiStatusString(status2) + "\n", "");
        return -2;
    }
    RaceLog::logInfo("RaceBoat", "accept success\n", "");

    //printf("\ntype message to send followed by <ctrl+d>\n");

    //message needs to be a hardcoded vector<uint8_t>
    //i.e. a vector of chars
    //std::string testMessage = "This is a test message";
    //std::vector<uint8_t> testVec(testMessage.begin(), testMessage.end());

    //auto message = readStdin();
    //auto message = testVec;
    std::string messageFromJavaClient = receiveMessageFromJavaClient(javaClientSocket);
    std::vector<uint8_t> clientMessageVec(messageFromJavaClient.begin(), messageFromJavaClient.end());
    auto message = clientMessageVec;
    std::string msgStr(message.begin(), message.end());

    auto [status5, received_message2] = connection.read_str();
    if (status5 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "read failed with status: " + getApiStatusString(status5) + "\n", "");
        status = status5;
    } else {
        RaceLog::logInfo("RaceBoat", "received message: " + std::string(received_message2.c_str()) + "\n", "");
        sendMessageToJavaClient(javaClientSocket, received_message2);
    }

    int packages_remaining = opts.num_packages;
    while (opts.num_packages == -1 || packages_remaining > 0) {
        auto status3 = connection.write_str(msgStr);
        if (status3 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "write failed with status: " + getApiStatusString(status3) + "\n", "");
            status = status3;
            break;
        } else {
            RaceLog::logInfo("RaceBoat", "wrote message: " + std::string(msgStr.c_str()) + "\n", "");
        }

        auto [status4, received_message] = connection.read_str();
        if (status4 != ApiStatus::OK) {
            RaceLog::logError("RaceBoat", "read failed with status: " + getApiStatusString(status4) + "\n", "");
            status = status4;
            break;
        } else {
            RaceLog::logInfo("RaceBoat", "received message: " + std::string(received_message.c_str()) + "\n", "");
            sendMessageToJavaClient(javaClientSocket, received_message);
        }
    }

    auto status6 = connection.close();
    if (status6 != ApiStatus::OK) {
        RaceLog::logError("RaceBoat", "close failed with status: " + getApiStatusString(status6) + "\n", "");
        status = status6;
    }

    return (status == ApiStatus::OK);
}

int runRaceboat(std::string configFilepath){
    std::optional<RaceboatOptions> opts;
    if (!configFilepath.empty()) {
        opts = parseOptsFromFile(configFilepath);
    }

    __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "MAIN CALLED");
    //might need to make some adjustments to return values so that failures and successes match what's expected
    opts = parseOpts();
    //artifact of optional type usage, removed currently
    //if (!opts.has_value()) {
    //    return 1;
    //}

    __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "opts gotten");
    RaceLog::setLogLevel(opts->log_level);
    int result = -1;


    int serverSocket = createServerSocket();
    __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "socket %d", serverSocket);
    if (serverSocket == -1) {
        return 1;
    }

    bool trigger = true;

    while (trigger) {
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "in accept loop");

        int clientSocket = acceptClientConnection(serverSocket);

        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "ACCEPT RETURNED!");
        //setup this way for initial socket testing
        //adjust this behavior once tested
        //to allow for repeated client connection without the server needing to be restarted
        if (clientSocket == -1) {
            close(serverSocket);
            trigger = false;
        }

        //check to see what mode the java client wants
        //Receive mode from the client
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "waiting to receive mode data");
        std::vector<uint8_t> modeData = readSocketData(clientSocket);
        std::vector<uint8_t> sendChannelData =readSocketData(clientSocket);
        std::vector<uint8_t> sendAddrData =readSocketData(clientSocket);
        std::vector<uint8_t> recvChannelData =readSocketData(clientSocket);
        std::vector<uint8_t> recvAddrData =readSocketData(clientSocket);
        std::vector<uint8_t> parametersData =readSocketData(clientSocket);

        std::string mode(modeData.begin(), modeData.end());
        std::string sendChannel(sendChannelData.begin(), sendChannelData.end());
        std::string sendAddr(sendAddrData.begin(), sendAddrData.end());
        std::string recvChannel(recvChannelData.begin(), recvChannelData.end());
        std::string recvAddr(recvAddrData.begin(), recvAddrData.end());
        std::string parameters(parametersData.begin(), parametersData.end());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "mode data %s", mode.c_str());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "sendChannel data %s", sendChannel.c_str());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "sendAddr data %s", sendAddr.c_str());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "recvChannel data %s", recvChannel.c_str());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "recvAddr data %s", recvAddr.c_str());
        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "parameters data %s", parameters.c_str());
        std::cout << "Received mode from client: " << mode << std::endl;

        //hardcode the mode or let the simple android ui choose it
        //adjust the empty string default behavior once tested
        if (!mode.empty()) {
            opts->mode = stringToMode(mode);
        } else {
            opts->mode = Mode::SEND_ONESHOT;
        }

        if (!sendChannel.empty()) {
            opts->send_channel = sendChannel;
        }
        if (!sendAddr.empty()) {
            opts->send_address = sendAddr;
        }
        if (!recvChannel.empty()) {
            opts->recv_channel = recvChannel;
        }
        if (!recvAddr.empty()) {
            opts->recv_address = recvAddr;
        }
        if (!parameters.empty()) {
            nlohmann::json jsonParams = nlohmann::json::parse(parameters);
            for (auto &kv: jsonParams.items()) {
                __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "parame: %s", kv.key().c_str());
                opts->params.push_back(std::make_pair(kv.key(), kv.value()));
            }
        }


        __android_log_print(ANDROID_LOG_DEBUG, "MainDriver", "send_address %s", opts->send_address.c_str());
        //switch into wanted mode and use the socket to get other necessary info from the java layer
        switch (opts->mode) {
            case Mode::SEND_ONESHOT:
                result = handle_send_oneshot(*opts, clientSocket);
                break;
            case Mode::SEND_RECV:
                result = handle_send_recv(*opts, clientSocket);
                break;
            case Mode::CLIENT_CONNECT:
                result = handle_client_connect(*opts, clientSocket);
                break;
            case Mode::RECV_RESPOND:
                result = handle_recv_respond(*opts, clientSocket);
                break;
            case Mode::RECV_ONESHOT:
                result = handle_recv_oneshot(*opts, clientSocket);
                break;
            case Mode::SERVER_CONNECT:
                result = handle_server_connect(*opts, clientSocket);
                break;
            default:
                RaceLog::logError("RaceBoat",
                                  "A mode must be selected [send, send-recv, client-connect, "
                                  "recv, recv-reply "
                                  "server-connect]\n",
                                  "");
        }
        //reaching here means the selected mode exited or there was an error
        close(clientSocket);
    }
    //flip the trigger to close the server socket and return a result
    //otherwise keep looping waiting for a new connection
    close(serverSocket);
    return result;
}

static int pipeFds[2];
static pthread_t stdioRedirectThread;

static void *stdioRedirectFunc(void*)
{
    size_t readSize;
    char buf[128];
    while ((readSize = read(pipeFds[0], buf, sizeof buf - 1)) > 0) {
        if (buf[readSize - 1] == '\n') {
            --readSize;
        }
        buf[readSize] = 0; // add null-terminator
        __android_log_write(ANDROID_LOG_ERROR, "stdio", buf);
    }
    return 0;
}

void redirectStdio(bool redirOut = false, bool redirErr = true) {
    pipe(pipeFds);

    if (redirOut) {
        setvbuf(stdout, 0, _IOLBF, 0); // line-buffered
        dup2(pipeFds[1], 1);
    }

    if (redirErr) {
        setvbuf(stderr, 0, _IONBF, 0); // unbuffered
        dup2(pipeFds[1], 2);
    }

    if (pthread_create(&stdioRedirectThread, 0, stdioRedirectFunc, 0) == -1) {
        __android_log_write(ANDROID_LOG_ERROR, "MainDriver", "Error creating stdio redirect thread");
        return;
    }
    pthread_detach(stdioRedirectThread);
}

int runRaceboat() {
    redirectStdio();
    return runRaceboat("");
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_twosixtech_raceboat_MainActivity_main(JNIEnv *env, jobject thiz) {
    // TODO delete this function after java structure is finalized
    return runRaceboat();
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_twosixtech_raceboat_RaceboatService_main(JNIEnv *env, jobject thiz) {
    return runRaceboat();
}