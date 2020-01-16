/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include "application/impl/configuration_storage_impl.hpp"
#include "core/runtime/runtime_test.hpp"
#include "crypto/hasher/hasher_impl.hpp"
#include "crypto/random_generator/boost_generator.hpp"
#include "crypto/sr25519/sr25519_provider_impl.hpp"
#include "extensions/impl/extension_factory_impl.hpp"
#include "runtime/binaryen/runtime_api/block_builder_impl.hpp"
#include "runtime/binaryen/runtime_api/core_impl.hpp"
#include "runtime/binaryen/wasm_memory_impl.hpp"
#include "runtime/common/storage_wasm_provider.hpp"
#include "storage/trie/impl/calculate_tree_root.hpp"
#include "storage/trie/impl/polkadot_trie_db.hpp"
#include "storage/trie/impl/polkadot_trie_db_backend.hpp"
#include "testutil/literals.hpp"
#include "testutil/outcome.hpp"
#include "testutil/runtime/common/basic_wasm_provider.hpp"

using kagome::application::ConfigurationStorage;
using kagome::application::ConfigurationStorageImpl;
using kagome::common::Buffer;
using kagome::crypto::BoostRandomGenerator;
using kagome::crypto::SR25519ProviderImpl;
using kagome::crypto::SR25519PublicKey;
using kagome::extensions::ExtensionFactoryImpl;
using kagome::primitives::Block;
using kagome::primitives::BlockHeader;
using kagome::primitives::BlockId;
using kagome::primitives::BlockNumber;
using kagome::primitives::Digest;
using kagome::primitives::DigestItem;
using kagome::primitives::DigestType;
using kagome::primitives::Extrinsic;
using kagome::runtime::WasmMemory;
using kagome::runtime::binaryen::BlockBuilderImpl;
using kagome::runtime::binaryen::CoreImpl;
using kagome::runtime::binaryen::WasmMemoryImpl;
using kagome::scale::encode;
using kagome::storage::trie::calculateOrderedTrieHash;
using kagome::storage::trie::calculateTrieRoot;

using testing::_;
using testing::Return;

namespace fs = boost::filesystem;

const auto AUTH_1 =
    SR25519PublicKey{{172, 133, 159, 138, 33,  110, 235, 27,  50, 11, 76,
                      118, 209, 24,  218, 61,  116, 7,   250, 82, 52, 132,
                      208, 169, 128, 18,  109, 59,  77,  13,  34, 10}};
const auto AUTH_2 =
    SR25519PublicKey{{18,  84,  247, 1,  127, 11,  131, 71,  206, 122, 177,
                      79,  150, 216, 24, 128, 46,  126, 158, 12,  13,  27,
                      124, 154, 203, 60, 114, 107, 8,   14,  122, 3}};

const auto ACCOUNT_1 =
    SR25519PublicKey{{64,  154, 0,   76,  160, 127, 0,  0,   50, 11, 76,
                      118, 209, 24,  218, 61,  116, 7,  250, 82, 52, 132,
                      208, 169, 128, 18,  109, 59,  77, 13,  34, 10}};
const auto ACCOUNT_2 =
    SR25519PublicKey{{18,  84,  247, 1,  127, 11,  131, 71,  206, 122, 177,
                      79,  150, 216, 24, 128, 46,  126, 158, 12,  13,  27,
                      124, 154, 203, 60, 114, 107, 8,   14,  122, 3}};

class RuntimeIntegrationTest : public testing::Test {
 public:
};

TEST_F(RuntimeIntegrationTest, ConstructGenesisWithRealWasm) {
  auto trie_db =
      std::shared_ptr(kagome::storage::trie::PolkadotTrieDb::createEmpty(
          std::make_shared<kagome::storage::trie::PolkadotTrieDbBackend>(
              std::make_shared<kagome::storage::InMemoryStorage>(),
              Buffer{},
              Buffer{0})));

  auto extension_factory =
      std::make_shared<kagome::extensions::ExtensionFactoryImpl>(trie_db);
  std::string wasm_path =
      boost::filesystem::path(__FILE__).parent_path().string()
      + "/wasm/substrate_test_runtime.compact.wasm";
  kagome::application::GenesisConfig genesis_config;
  genesis_config.authorities = {AUTH_1, AUTH_2};
  genesis_config.balances = {{ACCOUNT_1, 1000}, {ACCOUNT_2, 1000}};

  auto hasher = std::make_shared<kagome::crypto::HasherImpl>();
  EXPECT_OUTCOME_TRUE(
      raw_config,
      kagome::application::GenesisConfigEncoder(
          kagome::runtime::BasicWasmProvider(wasm_path)
              .getStateCode(),
          hasher)
          .encodeToRaw(genesis_config));
  for (auto &&[k, v] : raw_config) {
    EXPECT_OUTCOME_TRUE_1(trie_db->put(k, v));
  }

  auto wasm_provider =
      std::make_shared<kagome::runtime::StorageWasmProvider>(trie_db);

  auto core = std::make_shared<CoreImpl>(wasm_provider, extension_factory);

  // create genesis block
  Block genesis;
  genesis.header.number = 0;
  EXPECT_OUTCOME_TRUE(state_root, calculateTrieRoot(raw_config));
  std::copy_n(state_root.begin(),
              kagome::common::Hash256::size(),
              genesis.header.state_root.begin());

  EXPECT_OUTCOME_TRUE(genesis_extrinsics_root, calculateTrieRoot({}));
  std::copy_n(genesis_extrinsics_root.begin(),
              kagome::common::Hash256::size(),
              genesis.header.extrinsics_root.begin());

  EXPECT_OUTCOME_TRUE(genesis_header_bytes, encode(genesis.header));
  auto genesis_hash = hasher->blake2b_256(genesis_header_bytes);
  raw_config.emplace_back(hasher->twox_128("latest"_buf), genesis_hash);
  EXPECT_OUTCOME_TRUE_1(
      trie_db->put(raw_config.back().first, raw_config.back().second));

  Block block1;
  block1.header.number = 1;
  block1.header.parent_hash = genesis_hash;
  block1.header.state_root =
      "25e5b37074063ab75c889326246640729b40d0c86932edc527bc80db0e04fe5c"_hash256;

  EXPECT_OUTCOME_TRUE(extrinsics_root, calculateTrieRoot({}));
  std::copy_n(extrinsics_root.begin(),
              kagome::common::Hash256::size(),
              block1.header.extrinsics_root.begin());
  EXPECT_OUTCOME_TRUE_1(core->initialise_block(block1.header));

  auto block_builder =
      std::make_shared<BlockBuilderImpl>(wasm_provider, extension_factory);

  EXPECT_OUTCOME_TRUE(r, block_builder->finalise_block());
  ASSERT_EQ(r.number, 1);
  // TODO(Harrm) fix it
  //  EXPECT_OUTCOME_TRUE_1(core->execute_block(block1));
}
