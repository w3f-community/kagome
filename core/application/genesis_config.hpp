/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_APPLICATION_GENESIS_CONFIG_HPP
#define KAGOME_CORE_APPLICATION_GENESIS_CONFIG_HPP

#include "common/buffer.hpp"
#include "crypto/hasher.hpp"
#include "crypto/sr25519_types.hpp"
#include "crypto/util.hpp"
#include "scale/scale.hpp"

namespace kagome::application {

  using AuthorityId = crypto::SR25519PublicKey;
  using AccountId = crypto::SR25519PublicKey;

  /**
   * Configuration of a genesis block
   * Mostly used in tests as the actual application reads it from a file
   */
  struct GenesisConfig {
    std::vector<AuthorityId> authorities;
    std::vector<std::pair<AccountId, uint64_t>> balances;
    boost::optional<uint64_t> heap_pages_override;
  };

  // configurations from genesis.json lying under "genesis"->"raw" key
  using GenesisRawConfig =
      std::vector<std::pair<common::Buffer, common::Buffer>>;

  class GenesisConfigEncoder {
   public:
    GenesisConfigEncoder(common::Buffer wasm_runtime,
                         std::shared_ptr<crypto::Hasher> hasher)
        : wasm_runtime_{std::move(wasm_runtime)}, hasher_{std::move(hasher)} {}

    outcome::result<GenesisRawConfig> encodeToRaw(
        const GenesisConfig &config) const {
      GenesisRawConfig storage;

      for (auto &&[id, balance] : config.balances) {
        common::Buffer balance_key;
        balance_key.put("balance:");
        balance_key.put(id);
        auto val = common::Buffer{crypto::util::uint64_t_to_bytes(balance)};
        std::reverse(val.begin(), val.end());
        storage.emplace_back(hasher_->blake2b_256(balance_key), val);
      }
      storage.emplace_back(common::Buffer{}.put(":code"), wasm_runtime_);
      auto heap_pages_padding = common::Buffer{crypto::util::uint64_t_to_bytes(
          config.heap_pages_override.value_or(16))};  // default value
      std::reverse(heap_pages_padding.begin(), heap_pages_padding.end());
      storage.emplace_back(str2vec(":heappages"), heap_pages_padding);

      /*for (size_t i = 0; i < config.authorities.size(); i++) {
        common::Buffer auth_key;
        auth_key.put(":auth:");
        auth_key.put(crypto::util::uint32_t_to_bytes(i));
        std::reverse(auth_key.begin() + 6, auth_key.end());
        storage.emplace_back(auth_key, config.authorities.at(i));
      }*/
      OUTCOME_TRY(authorities, scale::encode(config.authorities));
      storage.emplace_back(hasher_->twox_128(str2vec("sys:auth")), authorities);

      /*/// TODO(Harrm) taken directly from substrate, needs to be investigated
      auto AUTH_KEY =
          common::Buffer{121, 192, 126, 43,  29,  46,  42,  191, 212, 133, 91,
                         147, 102, 23,  238, 255, 94,  6,   33,  196, 134, 154,
                         166, 12,  2,   190, 154, 220, 201, 138, 13,  29};
      storage.emplace_back(AUTH_KEY, authorities);*/
      return storage;
    }

   private:
    std::vector<uint8_t> str2vec(std::string_view str) const {
      std::vector<uint8_t> vec(str.size());
      std::copy_n(str.begin(), str.size(), vec.begin());
      return vec;
    }

    common::Buffer wasm_runtime_;
    std::shared_ptr<crypto::Hasher> hasher_;
  };

}  // namespace kagome::application

#endif  // KAGOME_CORE_APPLICATION_GENESIS_CONFIG_HPP
