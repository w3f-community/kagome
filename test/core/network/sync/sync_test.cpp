/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <gsl/span>

#include <libp2p/injector/host_injector.hpp>
#include <libp2p/protocol/identify.hpp>
#include <libp2p/basic/message_read_writer_uvarint.hpp>

#include "network/helpers/message_read_writer.hpp"
#include "network/adapters/protobuf_block_request.hpp"
#include "network/types/blocks_request.hpp"
#include "network/types/gossip_message.hpp"
#include "common/buffer.hpp"
#include "scale/scale.hpp"

#include "containers/objects_cache.hpp"

using namespace kagome; //NOLINT

struct Test1 {
  Test1() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
  }
  ~Test1() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
  }
};

KAGOME_DECLARE_CACHE(test,
                     KAGOME_CACHE_UNIT(std::string),
                     KAGOME_CACHE_UNIT(Test1),
                     KAGOME_CACHE_UNIT(std::vector<int>)
                     );
KAGOME_DEFINE_CACHE(test);

auto prepareBlockRequest() {
  auto from = primitives::BlockHash::fromHex("11111111111111111111111111111111"
                                             "22222222222222222222222222222222");

  network::BlocksRequest request;
  request.id = rand();
  request.fields = network::BlocksRequest::kBasicAttributes;
  request.from = from.value();
  request.max = 1;
  request.direction = network::Direction::ASCENDING;
  return request;
}

TEST(Syncing, SyncTest) {
  {
    auto ptr1 = KAGOME_EXTRACT_SHARED_CACHE(test, Test1);
    { auto ptr2 = KAGOME_EXTRACT_SHARED_CACHE(test, Test1); }
    auto ptr3 = KAGOME_EXTRACT_SHARED_CACHE(test, std::string);

    auto ptr4 = KAGOME_EXTRACT_RAW_CACHE(test, Test1);
    KAGOME_INSERT_RAW_CACHE(test, ptr4);
  }

  auto injector = libp2p::injector::makeHostInjector();
  auto host = injector.create<std::shared_ptr<libp2p::Host>>();

  auto context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  context->post([host{std::move(host)}] {  // NOLINT
    auto server_ma_res = libp2p::multi::Multiaddress::create(
        //"/ip4/127.0.0.1/tcp/30333/p2p/12D3KooWNoMM7DGZZiEoeTYmcmFMW16Xr3dfs2tbjE7GJdXgeeSb");  // NOLINT
        "/ip4/127.0.0.1/tcp/30333/p2p/12D3KooWEyoppNCUx8Yx66oV9fJnriXwCcXwDDUA2kj6vnc6iDEp");
    if (!server_ma_res) {
      std::cerr << "unable to create server multiaddress: "
                << server_ma_res.error().message() << std::endl;
      std::exit(EXIT_FAILURE);
    }

    std::string t{server_ma_res.value().getStringAddress()};
    auto server_ma = std::move(server_ma_res.value());

    auto server_peer_id_str = server_ma.getPeerId();
    if (!server_peer_id_str) {
      std::cerr << "unable to get peer id" << std::endl;
      std::exit(EXIT_FAILURE);
    }

    auto server_peer_id_res =
        libp2p::peer::PeerId::fromBase58(*server_peer_id_str);
    if (!server_peer_id_res) {
      std::cerr << "Unable to decode peer id from base 58: "
                << server_peer_id_res.error().message() << std::endl;
      std::exit(EXIT_FAILURE);
    }

    auto server_peer_id = std::move(server_peer_id_res.value());
    auto peer_info = libp2p::peer::PeerInfo{server_peer_id, {server_ma}};

    // libp2p::network::ConnectionManager cm;
    // auto manager = std::make_shared<libp2p::protocol::IdentifyMessageProcessor>(host, )

    // libp2p::protocol::Identify id()

    /*    host->newStream(peer_info, "/ipfs/id/1.0.0", [&](auto &&stream_res) {
          if (!stream_res)
            FAIL() << "Cannot connect to server: "
                   << stream_res.error().message();

          //stream_res.value() >> read_buf;
          //boost::outcome_v2::detail::basic_result_value_observers

          uint8_t s[1024*1024];
          gsl::span<uint8_t> out(s);

          std::shared_ptr<libp2p::connection::Stream> v = stream_res.value();
          v->read(out, 20, [](outcome::result<size_t> s) {
            size_t p = s.value();
            FAIL() << "Received: " << p;
          });


          int  p =0; ++p;
          //read_buf.resize(1024);
          //common::Buffer(read_buf).toHex();
        });*/
    /*    host->newStream(peer_info, "/ipfs/id/push/1.0.0", [&](auto
       &&stream_res) { if (!stream_res) FAIL() << "Cannot connect to server: "
                   << stream_res.error().message();

        });*/

    // targetPeerAddr, _ := ma.NewMultiaddr(fmt.Sprintf("/ipfs/%s", pid))
    // targetAddr := ipfsaddr.Decapsulate(targetPeerAddr)

    // We have a peer ID and a targetAddr so we add it to the peerstore
    // so LibP2P knows how to contact it
    // ha.Peerstore().AddAddr(peerid, targetAddr, peerstore.PermanentAddrTTL)

    host->getPeerRepository().getAddressRepository().addAddresses(
        server_peer_id,
        gsl::span<const libp2p::multi::Multiaddress>(&server_ma, 1),
        libp2p::peer::ttl::kPermanent);

    /*    host->newStream(peer_info, "/ipfs/id/1.0.0",
            [&identify](auto &&stream_res) {
              identify->handle(std::move(stream_res));
            });*/

    auto ma =
        libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/40010").value();
    auto listen_res = host->listen(ma);
    if (!listen_res) {
      std::cerr << "host cannot listen the given multiaddress: "
                << listen_res.error().message() << "\n";
      std::exit(EXIT_FAILURE);
    }

    host->start();

    host->newStream(peer_info, "/substrate/sup/6", [&](auto &&ss) {
      if (!ss)
        FAIL() << "Cannot connect to server: " << ss.error().message();

      std::cerr << "Connected" << std::endl;
      std::vector<uint8_t> rr = {
          0,   6,   0,   0,   0,   3,   0,   0,   0,   4,   0,   0,   0,   0,
          102, 17,  221, 133, 84,  21,  109, 36,  168, 197, 227, 165, 113, 47,
          246, 131, 108, 101, 146, 68,  122, 9,   176, 220, 143, 101, 136, 40,
          13,  143, 111, 255, 102, 17,  221, 133, 84,  21,  109, 36,  168, 197,
          227, 165, 113, 47,  246, 131, 108, 101, 146, 68,  122, 9,   176, 220,
          143, 101, 136, 40,  13,  143, 111, 255, 0};

      auto ssp = std::move(ss.value());
      auto rw = std::make_shared<libp2p::basic::MessageReadWriterUvarint>(ssp);
      rw->write(rr, [](auto write_res) {
        ASSERT_TRUE(write_res) << write_res.error().message();
      });
    });
  });

  try {
    // ping->start();
    context->run();
  } catch (std::exception &e) {
    std::cout << e.what();
  }

  int p = 0;
  ++p;
}