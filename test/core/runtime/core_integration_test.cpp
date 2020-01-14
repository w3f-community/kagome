/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "core/runtime/runtime_test.hpp"
#include "crypto/hasher/hasher_impl.hpp"
#include "crypto/random_generator/boost_generator.hpp"
#include "extensions/impl/extension_factory_impl.hpp"
#include "runtime/impl/core_impl.hpp"

using kagome::common::Buffer;
using kagome::crypto::BoostRandomGenerator;
using kagome::extensions::ExtensionFactoryImpl;
using kagome::primitives::Block;
using kagome::primitives::BlockHeader;
using kagome::primitives::BlockId;
using kagome::primitives::BlockNumber;
using kagome::primitives::Extrinsic;
using kagome::runtime::CoreImpl;
using kagome::runtime::WasmMemory;
using kagome::scale::encode;
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
