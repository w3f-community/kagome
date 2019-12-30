/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <runtime/impl/block_builder_api_impl.hpp>

#include "application/impl/configuration_storage_impl.hpp"
#include "core/runtime/runtime_test.hpp"
#include "core/storage/trie/mock_trie_db.hpp"
#include "crypto/hasher/hasher_impl.hpp"
#include "crypto/random_generator/boost_generator.hpp"
#include "crypto/sr25519/sr25519_provider_impl.hpp"
#include "extensions/impl/extension_factory_impl.hpp"
#include "runtime/impl/core_impl.hpp"
#include "runtime/impl/wasm_memory_impl.hpp"
#include "storage/trie/impl/calculate_tree_root.hpp"
#include "storage/trie/impl/polkadot_trie_db.hpp"
#include "storage/trie/impl/polkadot_trie_db_backend.hpp"
#include "testutil/literals.hpp"
#include "testutil/outcome.hpp"

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
using kagome::primitives::Extrinsic;
using kagome::runtime::BlockBuilderApiImpl;
using kagome::runtime::CoreImpl;
using kagome::runtime::WasmMemory;
using kagome::runtime::WasmMemoryImpl;
using kagome::scale::encode;
using kagome::storage::trie::calculateOrderedTrieHash;
using kagome::storage::trie::calculateTrieRoot;
using kagome::storage::trie::MockTrieDb;

using ::testing::_;
using ::testing::Return;

namespace fs = boost::filesystem;

class CoreTest : public RuntimeTest {
 public:
  void SetUp() override {
    RuntimeTest::SetUp();

    extension_factory_ =
        std::make_shared<kagome::extensions::ExtensionFactoryImpl>(trie_db_);
    std::string wasm_path =
        boost::filesystem::path(__FILE__).parent_path().string()
        + "/wasm/polkadot_runtime.compact.wasm";
    wasm_provider_ = std::make_shared<test::BasicWasmProvider>(wasm_path);

    core_ = std::make_shared<CoreImpl>(wasm_provider_, extension_factory_);
  }

 protected:
  std::shared_ptr<CoreImpl> core_;
};

/**
 * @given initialized core api
 * @when version is invoked
 * @then successful result is returned
 */
TEST_F(CoreTest, VersionTest) {
  ASSERT_TRUE(core_->version());
}

/**
 * @given initialized core api
 * @when execute_block is invoked
 * @then successful result is returned
 */
TEST_F(CoreTest, DISABLED_ExecuteBlockTest) {
  auto block = createBlock();

  ASSERT_TRUE(core_->execute_block(block));
}

/**
 * @given initialised core api
 * @when initialise_block is invoked
 * @then successful result is returned
 */
TEST_F(CoreTest, DISABLED_InitializeBlockTest) {
  auto header = createBlockHeader();

  ASSERT_TRUE(core_->initialise_block(header));
}

/**
 * @given initialized core api
 * @when authorities is invoked
 * @then successful result is returned
 */
TEST_F(CoreTest, DISABLED_AuthoritiesTest) {
  BlockId block_id = 0;
  ASSERT_TRUE(core_->authorities(block_id));
}

/**
 *
 */
TEST_F(CoreTest, ConstructGenesisWithRealWasm) {
  auto trie_db = std::shared_ptr(kagome::storage::trie::PolkadotTrieDb::createEmpty(
      std::make_shared<kagome::storage::trie::PolkadotTrieDbBackend>(
          std::make_shared<kagome::storage::InMemoryStorage>(),
          Buffer{},
          Buffer{0})));
  auto extension_factory =
      std::make_shared<kagome::extensions::ExtensionFactoryImpl>(trie_db);
  std::string wasm_path =
      boost::filesystem::path(__FILE__).parent_path().string()
      + "/wasm/polkadot_runtime.compact.wasm";
  auto wasm_provider = std::make_shared<test::BasicWasmProvider>(wasm_path);

  auto core = std::make_shared<CoreImpl>(wasm_provider, extension_factory);

  // create genesis config
  auto config_path = boost::filesystem::path(__FILE__).parent_path().string()
                     + "/wasm/genesis_custom.json";
  EXPECT_OUTCOME_TRUE(config_storage,
                      ConfigurationStorageImpl::create(config_path));
  auto generator = std::make_shared<BoostRandomGenerator>();
  SR25519ProviderImpl key_provider{generator};
  auto key1 = key_provider.generateKeypair().public_key;
  auto key2 = key_provider.generateKeypair().public_key;

  // create genesis block
  Block genesis;
  genesis.header.number = 0;
  EXPECT_OUTCOME_TRUE(state_root,
                      calculateTrieRoot(config_storage->getGenesis()));
  std::copy_n(state_root.begin(),
              kagome::common::Hash256::size(),
              genesis.header.state_root.begin());

  EXPECT_OUTCOME_TRUE(genesis_extrinsics_root, calculateTrieRoot({}));
  std::copy_n(genesis_extrinsics_root.begin(),
              kagome::common::Hash256::size(),
              genesis.header.extrinsics_root.begin());

  kagome::crypto::HasherImpl hasher;
  EXPECT_OUTCOME_TRUE(genesis_header_bytes, encode(genesis.header));
  auto genesis_hash = hasher.blake2s_256(genesis_header_bytes);

  Block block1;
  block1.header.number = 1;
  block1.header.parent_hash = genesis_hash;
  block1.header.state_root =
      "25e5b37074063ab75c889326246640729b40d0c86932edc527bc80db0e04fe5c"_hash256;
  Extrinsic tx{.data =
                   Buffer(kagome::scale::encode(key1, key2, 69, 0).value())};
  block1.body = {tx};
  std::vector<Buffer> txs(block1.body.size());
  std::transform(block1.body.begin(),
                 block1.body.end(),
                 txs.begin(),
                 [](auto const &extr) { return extr.data; });
  EXPECT_OUTCOME_TRUE(extrinsics_root,
                      calculateOrderedTrieHash(txs.begin(), txs.end()));
  std::copy_n(extrinsics_root.begin(),
              kagome::common::Hash256::size(),
              block1.header.extrinsics_root.begin());
  EXPECT_OUTCOME_TRUE_1(core->initialise_block(block1.header));

  auto block_builder =
      std::make_shared<BlockBuilderApiImpl>(wasm_provider_, extension_factory_);
git chec  EXPECT_OUTCOME_TRUE_1(block_builder->apply_extrinsic(tx));
  EXPECT_OUTCOME_TRUE_1(block_builder->finalise_block());

  // execute the block
  EXPECT_OUTCOME_TRUE_1(core->execute_block(block1));
}
