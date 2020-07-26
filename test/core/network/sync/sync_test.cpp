/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <libp2p/injector/host_injector.hpp>
#include "network/types/blocks_request.hpp"
#include "network/types/gossip_message.hpp"
#include "common/buffer.hpp"
#include "scale/scale.hpp"

using namespace kagome; //NOLINT

auto prepareBlockRequest() {
  network::BlocksRequest request;
  request.id = rand();
  request.fields = network::BlocksRequest::kBasicAttributes;
  request.from = 0;
  request.max = 5;
  request.direction = network::Direction::DESCENDING;
  return request;
}

TEST(Syncing, SyncTest) {
  auto injector = libp2p::injector::makeHostInjector();
  auto host = injector.create<std::shared_ptr<libp2p::Host>>();
  // create io_context - in fact, thing, which allows us to execute async
  // operations
  auto context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  context->post([host{std::move(host)}] {  // NOLINT
    auto server_ma_res =
        libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/30333/p2p/12D3KooWLLfVBqEK79xdh79U8nQQ2FrrTNpP1QEXgAS4ueuV5c55");  // NOLINT
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

    // create Host object and open a stream through it
    host->newStream(
        peer_info, "/sup/sync/2", [&](auto &&stream_res) {
          if (!stream_res) {
            FAIL() << "Cannot connect to server: "
                      << stream_res.error().message();
          }
          std::cerr << "Connected" << std::endl;
          auto request = prepareBlockRequest();
          network::GossipMessage message;
          message.type = network::GossipMessage::Type::BLOCK_REQUEST;
          message.data = common::Buffer(scale::encode(request).value());
          auto request_buf = scale::encode(message).value();
          auto stream_p = std::move(stream_res.value());
          stream_p->write(request_buf, request_buf.size(), [request_buf, stream_p](auto &&write_res){
            ASSERT_TRUE(write_res) << write_res.error().message();
            ASSERT_EQ(request_buf.size(), write_res.value());
            std::vector<uint8_t> read_buf{};
            read_buf.resize(1024);
            stream_p->read(read_buf, read_buf.size(), [read_buf](auto&& read_res){
              ASSERT_TRUE(read_res) << read_res.error().message();
              FAIL() << common::Buffer(read_buf).toHex();
            });
          });
//
//          auto echo_client = echo.createClient(stream_p);
//          std::cout << "SENDING 'Hello from C++!'\n";
//          echo_client->sendAnd(
//              "Hello from C++!\n",
//              [stream = std::move(stream_p)](auto &&response_result) {
//                std::cout << "RESPONSE " << response_result.value()
//                          << std::endl;
//                stream->close([](auto &&) { std::exit(EXIT_SUCCESS); });
//              });
        });
  });
  // run the IO context
  context->run();
}
