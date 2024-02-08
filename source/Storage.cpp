
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

#include "Storage.h"

#include <fstream>
#include <iterator>

#include "helper.h"

namespace Raceboat {

using namespace Raceboat;

Storage::~Storage() {}

std::vector<std::uint8_t> Storage::read(const std::filesystem::path &path) const {
    TRACE_METHOD();
    std::vector<std::uint8_t> fileContents;
    try {
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            uintmax_t fileSize = std::filesystem::file_size(path);
            fileContents.resize(fileSize);
            file.read(reinterpret_cast<char *>(fileContents.data()),
                      static_cast<std::streamsize>(fileSize));
            if (fileSize != fileContents.size()) {
                helper::logError(logPrefix + " read " + std::to_string(fileContents.size()) +
                                 " of " + std::to_string(fileSize) + " bytes");
            }
            if (isFileEncryptable(path)) {
                return decrypt(fileContents);
            }
        } else {
            helper::logWarning(logPrefix + " cannot open file: " + path.string());
        }
    } catch (const std::exception &ex) {
        helper::logError(logPrefix + " exception: " + ex.what());
    } catch (...) {
        helper::logError(logPrefix + "unknown exception");
    }

    return fileContents;
}

bool Storage::write(const std::filesystem::path &path, const std::vector<std::uint8_t> &data) {
    TRACE_METHOD();
    try {
        std::ofstream file(path.c_str(), std::ios_base::trunc);
        if (file.is_open()) {
            if (data.size()) {
                if (isFileEncryptable(path)) {
                    auto encryptedData = encrypt(data);
                    file.write(reinterpret_cast<const char *>(&encryptedData[0]),
                               static_cast<std::streamsize>(encryptedData.size()));
                } else {
                    file.write(reinterpret_cast<const char *>(&data[0]),
                               static_cast<std::streamsize>(data.size()));
                }
            }
            return true;
        }
    } catch (const std::exception &ex) {
        helper::logError(logPrefix + " exception: " + ex.what());
    } catch (...) {
        helper::logError(logPrefix + "unknown exception");
    }
    return false;
}

bool Storage::append(const std::filesystem::path &path, const std::vector<std::uint8_t> &data) {
    TRACE_METHOD();
    try {
        if (!std::filesystem::exists(path)) {
            return this->write(path, data);
        } else {
            std::ofstream file(path, std::ios_base::app);
            if (file.is_open()) {
                if (data.size()) {
                    if (isFileEncryptable(path)) {
                        return appendCiphertext(path, data);
                    } else {
                        file.write(reinterpret_cast<const char *>(&data[0]),
                                   static_cast<std::streamsize>(data.size()));
                    }
                } else {
                    helper::logInfo(logPrefix + " no data");
                }
                return true;
            } else {
                helper::logInfo(logPrefix + " could not open file " + path.string());
            }
        }
    } catch (const std::exception &ex) {
        helper::logError(logPrefix + " exception: " + ex.what());
    } catch (...) {
        helper::logError(logPrefix + "unknown exception");
    }
    return false;
}

bool Storage::isFileEncryptable(const std::filesystem::path &) const {
    return false;
}

std::vector<std::uint8_t> Storage::decrypt(const std::vector<std::uint8_t> &ciphertext) const {
    return ciphertext;
}

std::vector<std::uint8_t> Storage::encrypt(const std::vector<std::uint8_t> &plaintext) const {
    return plaintext;
}

bool Storage::appendCiphertext(const std::filesystem::path &existingEncryptedFile,
                               const std::vector<std::uint8_t> &plaintext) const {
    MAKE_LOG_PREFIX();
    bool success = false;
    try {
        std::ofstream file(existingEncryptedFile, std::ios_base::app);
        if (file.is_open()) {
            auto existingCiphertext = read(existingEncryptedFile);
            auto combinedPlainText = decrypt(existingCiphertext);
            combinedPlainText.insert(combinedPlainText.end(), plaintext.begin(), plaintext.end());
            std::vector<std::uint8_t> combinedCiphertext = encrypt(combinedPlainText);
            file.write(reinterpret_cast<char *>(&combinedCiphertext[0]),
                       static_cast<std::streamsize>(combinedCiphertext.size()));
            success = true;
        }
    } catch (const std::exception &ex) {
        helper::logError(logPrefix + " exception: " + ex.what());
    } catch (...) {
        helper::logError(logPrefix + "unknown exception");
    }
    return success;
}

}  // namespace Raceboat
