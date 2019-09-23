/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libp2p/host_builder.hpp"

#include "libp2p/crypto/key_generator/key_generator_impl.hpp"
#include "libp2p/crypto/key_validator/key_validator_impl.hpp"
#include "libp2p/crypto/random_generator/boost_generator.hpp"
#include "libp2p/host/basic_host.hpp"
#include "libp2p/muxer/yamux.hpp"
#include "libp2p/network/default_network.hpp"
#include "libp2p/peer/address_repository/inmem_address_repository.hpp"
#include "libp2p/peer/impl/peer_repository_impl.hpp"
#include "libp2p/peer/key_repository/inmem_key_repository.hpp"
#include "libp2p/peer/protocol_repository/inmem_protocol_repository.hpp"
#include "libp2p/security/plaintext.hpp"
#include "libp2p/transport/impl/upgrader_impl.hpp"
#include "libp2p/transport/tcp.hpp"

// TODO(akvinikym) PRE-215 27.06.19: revert multiselect
//#include "libp2p/protocol_muxer/multiselect.hpp"

namespace {
  /**
   * Check if both keys in the keypair have data and type set
   * @param keypair to be checked
   * @return true, if keypair is well-formed, false otherwise
   */
  bool keypairIsWellFormed(const libp2p::crypto::KeyPair &keypair) {
    using KeyType = libp2p::crypto::Key::Type;

    const auto &pubkey = keypair.publicKey;
    const auto &privkey = keypair.privateKey;
    return !pubkey.data.empty() && pubkey.type != KeyType::UNSPECIFIED
           && !privkey.data.empty() && privkey.type == pubkey.type;
  }
}  // namespace

namespace libp2p {
  using multi::Multiaddress;

  HostBuilder::HostBuilder(Config &&config) : config_{std::move(config)} {}

  HostBuilder &HostBuilder::setKeypair(const crypto::KeyPair &kp) {
    config_.peer_key = kp;
    return *this;
  }

  HostBuilder &HostBuilder::setCSPRNG(detail::sptr<crypto::random::CSPRNG> r) {
    config_.cprng = std::move(r);
    return *this;
  }

  HostBuilder &HostBuilder::setPRNG(
      detail::sptr<crypto::random::RandomGenerator> r) {
    config_.prng = std::move(r);
    return *this;
  }

  HostBuilder &HostBuilder::setPeerRepository(
      detail::uptr<peer::PeerRepository> p) {
    config_.peer_repository = std::move(p);
    return *this;
  }

  HostBuilder &HostBuilder::addTransport(
      detail::sptr<transport::TransportAdaptor> tr) {
    config_.transports.push_back(std::move(tr));
    return *this;
  }

  HostBuilder &HostBuilder::addMuxerAdaptor(
      detail::sptr<muxer::MuxerAdaptor> mux) {
    config_.muxers.push_back(std::move(mux));
    return *this;
  }

  HostBuilder &HostBuilder::addSecurityAdaptor(
      detail::sptr<security::SecurityAdaptor> s) {
    config_.securities.push_back(std::move(s));
    return *this;
  }

  HostBuilder &HostBuilder::addListenMultiaddr(
      const multi::Multiaddress &address) {
    config_.listen_addresses.push_back(address);
    return *this;
  }

  HostBuilder &HostBuilder::addListenMultiaddr(std::string_view address) {
    multiaddr_candidates_.push_back(address);
    return *this;
  }

  HostBuilder &HostBuilder::setContext(
      std::shared_ptr<boost::asio::execution_context> c) {
    config_.context = std::move(c);
    return *this;
  }

  outcome::result<std::shared_ptr<Host>> HostBuilder::build() {
    for (auto multiaddr_candidate : multiaddr_candidates_) {
      OUTCOME_TRY(addr, Multiaddress::create(multiaddr_candidate));
      config_.listen_addresses.push_back(std::move(addr));
    }

    if (!config_.cprng) {
      config_.cprng = std::make_shared<crypto::random::BoostRandomGenerator>();
    }

    auto key_generator =
        std::make_shared<crypto::KeyGeneratorImpl>(*config_.cprng);

    if (keypairIsWellFormed(config_.peer_key)) {
      OUTCOME_TRY(keys,
                  key_generator->generateKeys(crypto::Key::Type::RSA));
      config_.peer_key = std::move(keys);
    }

    auto idmgr = std::make_shared<peer::IdentityManagerImpl>(config_.peer_key);

    auto multiselect = std::make_shared<protocol_muxer::Multiselect>();

    auto router = std::make_shared<network::RouterImpl>();

    auto key_validator = std::make_shared<crypto::validator::KeyValidatorImpl>(
        std::move(key_generator));

    auto marshaller = std::make_shared<crypto::marshaller::KeyMarshallerImpl>(
        std::move(key_validator));

    std::vector<std::shared_ptr<security::SecurityAdaptor>> security_adaptors =
        {std::make_shared<security::Plaintext>(std::move(marshaller), idmgr)};

    muxer::MuxedConnectionConfig muxed_config_{1024576, 1000};

    std::vector<std::shared_ptr<muxer::MuxerAdaptor>> muxer_adaptors = {
        std::make_shared<muxer::Yamux>(muxed_config_)};

    auto upgrader = std::make_shared<transport::UpgraderImpl>(
        multiselect, std::move(security_adaptors), std::move(muxer_adaptors));

    std::vector<std::shared_ptr<transport::TransportAdaptor>> transports =
        config_.transports;

    auto tmgr =
        std::make_shared<network::TransportManagerImpl>(std::move(transports));

    auto cmgr = std::make_shared<network::ConnectionManagerImpl>(tmgr);

    auto listener = std::make_unique<network::ListenerManagerImpl>(
        multiselect, std::move(router), tmgr, cmgr);

    auto dialer =
        std::make_unique<network::DialerImpl>(multiselect, tmgr, cmgr);

    auto network = std::make_unique<network::NetworkImpl>(
        std::move(listener), std::move(dialer), cmgr);

    auto addr_repo = std::make_shared<peer::InmemAddressRepository>();

    auto key_repo = std::make_shared<peer::InmemKeyRepository>();

    auto protocol_repo = std::make_shared<peer::InmemProtocolRepository>();

    auto peer_id = peer::PeerId::fromPublicKey(config_.peer_key.publicKey.data);

    //

    if (!config_.peer_repository) {
      config_.peer_repository = std::make_unique<peer::PeerRepositoryImpl>(
          std::make_shared<peer::InmemAddressRepository>(),
          std::make_shared<peer::InmemKeyRepository>(),
          std::make_shared<peer::InmemProtocolRepository>());
    }

    auto io_context = std::make_shared<boost::asio::io_context>(1);
    if (!config_.context) {
      config_.context = io_context;
    }
    //    return Host{config_, std::move(peer_id)};

    return std::make_shared<host::BasicHost>(
        idmgr, std::move(network), std::move(config_.peer_repository));
  }
}  // namespace libp2p
