/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libp2p/crypto/key_marshaller/key_marshaller_impl.hpp"

#include "libp2p/crypto/common.hpp"
#include "libp2p/crypto/key_generator.hpp"
#include "libp2p/crypto/protobuf/keys.pb.h"

namespace libp2p::crypto::marshaller {
  namespace {
    /**
     * @brief converts Key::Type to protobuf::KeyType
     * @param type common key type value
     * @return protobuf key type value
     */
    outcome::result<protobuf::KeyType> marshalKeyType(Key::Type type) {
      switch (type) {
        case Key::Type::RSA:
          return protobuf::KeyType::RSA;
        case Key::Type::Ed25519:
          return protobuf::KeyType::Ed25519;
        case Key::Type::Secp256k1:
          return protobuf::KeyType::Secp256k1;
      }

      return CryptoProviderError::UNKNOWN_KEY_TYPE;
    }

    /**
     * @brief converts protobuf::KeyType to Key::Type
     * @param type protobuf key type value
     * @return common key type value
     */
    outcome::result<Key::Type> unmarshalKeyType(protobuf::KeyType type) {
      switch (type) {
        case protobuf::KeyType::RSA:
          return Key::Type::RSA;
        case protobuf::KeyType::Ed25519:
          return Key::Type::Ed25519;
        case protobuf::KeyType::Secp256k1:
          return Key::Type::Secp256k1;
        default:
          return CryptoProviderError::UNKNOWN_KEY_TYPE;
      }
    }
  }  // namespace

  KeyMarshallerImpl::KeyMarshallerImpl(
      std::shared_ptr<validator::KeyValidator> key_validator)
      : key_validator_{std::move(key_validator)} {}

  outcome::result<KeyMarshallerImpl::ByteArray> KeyMarshallerImpl::marshal(
      const PublicKey &key) const {
    protobuf::PublicKey protobuf_key;
    OUTCOME_TRY(type, marshalKeyType(key.type));
    protobuf_key.set_type(type);
    protobuf_key.set_data(key.data.data(), key.data.size());

    auto string = protobuf_key.SerializeAsString();
    KeyMarshallerImpl::ByteArray out(string.begin(), string.end());
    return KeyMarshallerImpl::ByteArray(string.begin(), string.end());
  }

  outcome::result<KeyMarshallerImpl::ByteArray> KeyMarshallerImpl::marshal(
      const PrivateKey &key) const {
    // TODO(Harrm): Check if it's a typo
    protobuf::PrivateKey protobuf_key;
    OUTCOME_TRY(type, marshalKeyType(key.type));
    protobuf_key.set_type(type);
    protobuf_key.set_data(key.data.data(), key.data.size());

    auto string = protobuf_key.SerializeAsString();
    return KeyMarshallerImpl::ByteArray(string.begin(), string.end());
  }

  outcome::result<PublicKey> KeyMarshallerImpl::unmarshalPublicKey(
      const KeyMarshallerImpl::ByteArray &key_bytes) const {
    protobuf::PublicKey protobuf_key;
    if (!protobuf_key.ParseFromArray(key_bytes.data(), key_bytes.size())) {
      return CryptoProviderError::FAILED_UNMARSHAL_DATA;
    }

    OUTCOME_TRY(type, unmarshalKeyType(protobuf_key.type()));
    KeyMarshallerImpl::ByteArray data(protobuf_key.data().begin(),
                                      protobuf_key.data().end());
    auto key = PublicKey{{type, data}};
    OUTCOME_TRY(key_validator_->validate(key));

    return key;
  }

  outcome::result<PrivateKey> KeyMarshallerImpl::unmarshalPrivateKey(
      const KeyMarshallerImpl::ByteArray &key_bytes) const {
    protobuf::PublicKey protobuf_key;
    if (!protobuf_key.ParseFromArray(key_bytes.data(), key_bytes.size())) {
      return CryptoProviderError::FAILED_UNMARSHAL_DATA;
    }

    OUTCOME_TRY(type, unmarshalKeyType(protobuf_key.type()));
    KeyMarshallerImpl::ByteArray data(protobuf_key.data().begin(),
                                      protobuf_key.data().end());
    auto key = PrivateKey{{type, data}};
    OUTCOME_TRY(key_validator_->validate(key));

    return key;
  }

}  // namespace libp2p::crypto::marshaller
