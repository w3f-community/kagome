/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CONFIG_HPP
#define KAGOME_CONFIG_HPP

#include <memory>
#include <vector>

#include <boost/asio/io_context.hpp>
#include "libp2p/crypto/key.hpp"
#include "libp2p/crypto/random_generator.hpp"
#include "libp2p/muxer/muxer_adaptor.hpp"
#include "libp2p/peer/peer_id.hpp"
#include "libp2p/peer/peer_repository.hpp"
#include "libp2p/protocol_muxer/protocol_muxer.hpp"
#include "libp2p/security/security_adaptor.hpp"
#include "libp2p/transport/transport_adaptor.hpp"
#include "libp2p/transport/upgrader.hpp"

namespace libp2p {

  namespace detail {
    template <typename T>
    using vec = std::vector<T>;

    template <typename T>
    using sptr = std::shared_ptr<T>;

    template<typename T>
    using uptr = std::unique_ptr<T>;

    template <typename T>
    using vecsptr = vec<sptr<T>>;
  }  // namespace detail

  /**
   * Configuration of Libp2p Host
   */
  struct Config {
    crypto::KeyPair peer_key;
    detail::sptr<crypto::random::CSPRNG> cprng;
    detail::sptr<crypto::random::RandomGenerator> prng;
    detail::uptr<peer::PeerRepository> peer_repository;
    detail::vecsptr<transport::TransportAdaptor> transports;
    detail::vecsptr<muxer::MuxerAdaptor> muxers;
    detail::vecsptr<security::SecurityAdaptor> securities;
    detail::vec<multi::Multiaddress> listen_addresses;

    detail::sptr<boost::asio::execution_context> context;
    detail::sptr<boost::asio::io_context::executor_type> executor;
    detail::sptr<protocol_muxer::ProtocolMuxer> protocol_muxer;
    detail::sptr<transport::Upgrader> upgrader;

    bool enable_ping = true;
  };

}  // namespace libp2p

#endif  // KAGOME_CONFIG_HPP

