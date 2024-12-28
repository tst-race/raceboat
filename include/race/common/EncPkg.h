
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

#ifndef ENC_PKG
#define ENC_PKG

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "PackageType.h"

using RawData = std::vector<std::uint8_t>;

/**
 * @brief Class for representing an encrypted package in the RACE system.
 *
 */
class EncPkg {
private:
  // for compatibility with RACE
  uint64_t traceId;
  uint64_t spanId;
  uint8_t packageType;

  // The only one the comm lib cares about
  RawData cipherText;

public:
  /**
   * @brief Construct an encrypted package using the provided trace ID, span ID,
   * and cipher text.
   *
   * @param traceId The trace ID.
   * @param spanId The span ID.
   * @param cipherText The cipher text.
   */
  EncPkg(uint64_t traceId, uint64_t spanId, const RawData &cipherText);

  /**
   * @brief Construct an encrypted package from the raw data of another
   * encrypted package. Expected form of the incoming raw data should be an
   * appended byte array of trace ID, span ID, package type, and cipher text IN
   * THAT ORDER. This constructor expects the raw data format of the return
   * value of EncPkg::getRawData(). So, for example, one could copy an encrypted
   * package by doing this:
   *     ```
   *     EncPkg newEncryptedPackage(oldEncryptedPackage.getRawData());
   *     ```
   *
   * @param rawData An appended byte array of trace ID, span ID, package type,
   * and cipher text IN THAT ORDER.
   */
  explicit EncPkg(RawData rawData);

  /**
   * @brief Get the package in the form of raw bytes
   *
   * @return RawData The bytes contained in this package
   */
  RawData getRawData() const;

  /**
   * @brief Identical to getRawData in the context of the comm lib
   *
   * @return RawData The bytes contained in this package
   */
  RawData getCipherText() const;

  /**
   * @brief Get the trace ID of the encrypted package.
   *
   * @return uint64_t The trace ID.
   */
  uint64_t getTraceId() const;

  /**
   * @brief Get the span ID of the encrypted package.
   *
   * @return uint64_t The span ID.
   */
  uint64_t getSpanId() const;

  /**
   * @brief Get the package type of the encrypted package.
   *
   * The package type is set automatically by the SDK to differentiate between
   * packages sent by a NetworkManager plugin and the test harness. This field
   * is irrelevant to a NetworkManager plugin, and Comms channels do not need to
   * handle it so long as they are using the raw data of the package.
   *
   * @return Package type
   */
  PackageType getPackageType() const;

  /**
   * @brief Set the trace ID for the encrypted package.
   *
   * @param value
   */
  void setTraceId(uint64_t value);

  /**
   * @brief Set the span ID for the encrypted package.
   *
   * @param value
   */
  void setSpanId(uint64_t value);

  /**
   * @brief Set the package type for the encrypted package.
   *
   * @param value Package type
   */
  void setPackageType(PackageType value);

  /**
   * @brief Gets the total size of this package, in bytes.
   *
   * This sums the sizes of the individual components instead of calling
   * getRawData to prevent the copy that would occur inside of getRawData.
   *
   * @return Size in bytes
   */
  size_t getSize() const;
};

inline bool operator==(const EncPkg &lhs, const EncPkg &rhs) {
  return lhs.getCipherText() == rhs.getCipherText();
}

inline bool operator!=(const EncPkg &lhs, const EncPkg &rhs) {
  return !operator==(lhs, rhs);
}

#endif
