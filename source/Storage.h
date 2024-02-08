
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

#include <filesystem>
#include <string>
#include <vector>

namespace Raceboat {

/**
 * @brief Storage class designed for encryption algorithms
 * specializations should implement the protected encryption-related methods
 **/
class Storage {
public:
    virtual ~Storage();

    /**
     * @brief read file
     *
     * @param path Absolute file path
     * @return all file data
     */
    std::vector<std::uint8_t> read(const std::filesystem::path &path) const;

    /**
     * @brief write file
     *
     * @param path Absolute file path
     * @param data Byte vector to write to file
     * @return true if successful, otherwise false
     */
    bool write(const std::filesystem::path &path, const std::vector<std::uint8_t> &data);

    /**
     * @brief append file
     *
     * @param path Absolute file path
     * @param data Byte vector to append to file
     * @return true if successful, otherwise false
     */
    bool append(const std::filesystem::path &path, const std::vector<std::uint8_t> &data);

protected:
    virtual bool isFileEncryptable(const std::filesystem::path &path) const;
    virtual std::vector<std::uint8_t> decrypt(const std::vector<std::uint8_t> &ciphertext) const;
    virtual std::vector<std::uint8_t> encrypt(const std::vector<std::uint8_t> &plaintext) const;

    // allow user to optimize appending to an encrypted file (implementation specific optimization)
    // base implemenation is rather brute force (read entire file, decrypt, append new plaintext,
    // write)
    virtual bool appendCiphertext(const std::filesystem::path &existingEncryptedFile,
                                  const std::vector<std::uint8_t> &plaintext) const;
};
}  // namespace Raceboat