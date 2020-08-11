/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <gsl/span>

#include <libp2p/injector/host_injector.hpp>
#include <libp2p/protocol/identify.hpp>
#include "network/types/blocks_request.hpp"
#include "network/types/gossip_message.hpp"
#include "common/buffer.hpp"
#include "scale/scale.hpp"

using namespace kagome; //NOLINT

auto prepareBlockRequest() {
  network::BlocksRequest request;
  request.id = rand();
  request.fields = network::BlocksRequest::kBasicAttributes;
  request.from = 5;
  request.max = 5;
  request.direction = network::Direction::ASCENDING;
  return request;
}

TEST(Syncing, SyncTest) {
  auto injector = libp2p::injector::makeHostInjector();
  auto host = injector.create<std::shared_ptr<libp2p::Host>>();

  auto identify = injector.create<std::shared_ptr<libp2p::protocol::Identify>>();
  identify->start();

  // create io_context - in fact, thing, which allows us to execute async
  // operations
  auto context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  context->post([host{std::move(host)}] {  // NOLINT
    auto server_ma_res = libp2p::multi::Multiaddress::create(
        "/ip4/127.0.0.1/tcp/30333/p2p/12D3KooWNoMM7DGZZiEoeTYmcmFMW16Xr3dfs2tbjE7GJdXgeeSb");  // NOLINT
    if (!server_ma_res) {
      std::cerr << "unable to create server multiaddress: "
                << server_ma_res.error().message() << std::endl;
      std::exit(EXIT_FAILURE);
    }
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
    host->newStream(peer_info, "/sup/sync/2", [&](auto &&stream_res) {
      if (!stream_res) {
        FAIL() << "Cannot connect to server: " << stream_res.error().message();
      }
      std::cerr << "Connected" << std::endl;

      auto request = prepareBlockRequest();
      network::GossipMessage message;
      message.type = network::GossipMessage::Type::BLOCK_REQUEST;
      //message.data = common::Buffer(scale::encode(request).value());
      message.data = common::Buffer(scale::encode(request).value());
      auto request_buf = scale::encode(message).value();
      auto stream_p = std::move(stream_res.value());

      std::vector<uint8_t> read_buf{};
      read_buf.resize(10);
      stream_p->read(read_buf, 10, [read_buf, stream_p](auto &&read_res) {
        FAIL() << "Read res: " << read_res.error().message();

        ASSERT_TRUE(read_res) << read_res.error().message();
        FAIL() << common::Buffer(read_buf).toHex();
      });

      //strcpy((char*)&request_buf[0], "Hello!!!!!");

      std::cerr << std::endl;
      for (auto b : request_buf) {
        std::cerr << "0x" << std::hex << static_cast<int>(b) << ", ";
      }

      stream_p->write(
          request_buf,
          request_buf.size(),
          [request_buf, stream_p](auto &&write_res) {
/*            std::vector<uint8_t> read_buf{};
            read_buf.resize(10);
            stream_p->read(read_buf, 10, [read_buf, stream_p](auto &&read_res) {
              FAIL() << "Read res: " << read_res.error().message();

              ASSERT_TRUE(read_res) << read_res.error().message();
              FAIL() << common::Buffer(read_buf).toHex();
            });*/

            //ASSERT_TRUE(write_res) << write_res.error().message();
            //ASSERT_EQ(request_buf.size(), write_res.value());
          });
    });
  });

  context->run();
}
