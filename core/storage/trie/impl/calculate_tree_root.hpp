/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CALCULATE_TREE_ROOT_HPP
#define KAGOME_CALCULATE_TREE_ROOT_HPP

#include "common/buffer.hpp"
#include "crypto/hasher.hpp"
#include "scale/scale.hpp"
#include "storage/in_memory/in_memory_storage.hpp"
#include "storage/trie/impl/polkadot_codec.hpp"
#include "storage/trie/impl/polkadot_trie.hpp"
#include "storage/trie/impl/polkadot_trie_db.hpp"
#include "storage/trie/impl/polkadot_trie_db_backend.hpp"

namespace kagome::storage::trie {

  /**
   * Instantiate empty merkle trie, insert \param key_vals pairs and \return
   * Buffer containing merkle root of resulting trie
   */
  static outcome::result<common::Buffer> calculateTrieRoot(
      const std::vector<std::pair<common::Buffer, common::Buffer>> &key_vals) {
    auto trie_db = storage::trie::PolkadotTrieDb::createEmpty(
        std::make_shared<storage::trie::PolkadotTrieDbBackend>(
            std::make_shared<storage::InMemoryStorage>(),
            common::Buffer{},
            common::Buffer{0}));

    for (const auto &[key, val] : key_vals) {
      OUTCOME_TRY(trie_db->put(key, val));
    }
    return trie_db->getRootHash();
  }

  /**
   * Calculates the hash of a Merkle tree containing the items from the provided
   * range [begin; end) as values and compact-encoded indices of those
   * values(starting from 0) as keys
   * @tparam It an iterator type of a container of common::Buffers
   * @return the Merkle tree root hash of the tree containing provided values
   */
  template <typename It>
  outcome::result<common::Buffer> calculateOrderedTrieHash(const It &begin,
                                                           const It &end) {
    PolkadotTrie trie;
    PolkadotCodec codec;
    // empty root
    if (begin == end) return common::Buffer{}.put(codec.hash256({0}));

    // clang-format off
    static_assert(
        std::is_same_v<std::decay_t<decltype(*begin)>, common::Buffer>);
    // clang-format on
    It it = begin;
    scale::CompactInteger key = 0;
    while (it != end) {
      OUTCOME_TRY(enc, scale::encode(key++));
      OUTCOME_TRY(trie.put(common::Buffer{enc}, *it));
      it++;
    }
    OUTCOME_TRY(enc, codec.encodeNode(*trie.getRoot()));
    auto merkleValue = codec.merkleValue(enc);
    auto merkleHash = merkleValue.size() < 32
                          ? common::Buffer{codec.hash256(merkleValue)}
                          : merkleValue;
    return merkleHash;
  }

}  // namespace kagome::storage::trie

#endif  // KAGOME_CALCULATE_TREE_ROOT_HPP
