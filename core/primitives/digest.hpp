/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_PRIMITIVES_DIGEST_HPP
#define KAGOME_CORE_PRIMITIVES_DIGEST_HPP

#include "common/buffer.hpp"

namespace kagome::primitives {
  /**
   * Digest is an implementation- and usage-defined entity, for example,
   * information, needed to verify the block
   */
  enum class DigestType: uint32_t {
    Other = 0,
    AuthoritiesChange = 1,
    ChangesTrieRoot = 2,
    Seal = 3,
    Consensus = 4
  };

  struct DigestItem {
    DigestItem() = default;
    explicit DigestItem(DigestType t) : type {t}, data{} {}
    DigestItem(DigestType t, common::Buffer b) : type {t}, data{std::move(b)} {}

    bool operator==(const DigestItem& item) const {
      return type == item.type and data == item.data;
    }

    bool operator!=(const DigestItem& item) const {
      return not(*this == item);
    }

    DigestType type = DigestType::Other;
    common::Buffer data;
  };

  /**
   * @brief outputs object of type DigestItem to stream
   * @tparam Stream output stream type
   * @param s stream reference
   * @param v value to output
   * @return reference to stream
   */
  template <class Stream,
      typename = std::enable_if_t<Stream::is_encoder_stream>>
  Stream &operator<<(Stream &s, const DigestItem &b) {
    return s << static_cast<uint32_t>(b.type) << b.data;
  }

  /**
   * @brief decodes object of type DigestItem from stream
   * @tparam Stream input stream type
   * @param s stream reference
   * @param v value to decode
   * @return reference to stream
   */
  template <class Stream,
      typename = std::enable_if_t<Stream::is_decoder_stream>>
  Stream &operator>>(Stream &s, DigestItem &b) {
    uint32_t type {};
    s >> type >> b.data;
    b.type = static_cast<DigestType>(type);
    return s;
  }

  using Digest = std::vector<DigestItem>;

}  // namespace kagome::primitives

#endif  // KAGOME_CORE_PRIMITIVES_DIGEST_HPP
