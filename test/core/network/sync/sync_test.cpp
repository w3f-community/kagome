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

    //targetPeerAddr, _ := ma.NewMultiaddr(fmt.Sprintf("/ipfs/%s", pid))
    //targetAddr := ipfsaddr.Decapsulate(targetPeerAddr)

    // We have a peer ID and a targetAddr so we add it to the peerstore
    // so LibP2P knows how to contact it
    //ha.Peerstore().AddAddr(peerid, targetAddr, peerstore.PermanentAddrTTL)

    host->getPeerRepository().getAddressRepository().addAddresses(
        server_peer_id, gsl::span<const libp2p::multi::Multiaddress>(&server_ma, 1), libp2p::peer::ttl::kPermanent);

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

    host->newStream(peer_info, "/sup/sync/2", [&](auto &&stream_res) {
      if (!stream_res) {
        FAIL() << "Cannot connect to server: " << stream_res.error().message();
      }
      std::cerr << "Connected" << std::endl;

      auto request = prepareBlockRequest();
      std::vector<uint8_t> request_buf;

      using ProtobufRW = network::MessageReadWriter<network::ProtobufMessageAdapter<network::BlocksRequest>, network::NoSink>;
      auto it = ProtobufRW::write(request, request_buf);

      gsl::span<uint8_t> data(it.base(),
                              request_buf.size() - std::distance(request_buf.begin(), it));
      assert(!data.empty());

/*      std::vector<uint8_t> request_buf = {
          44, 8, 128, 128, 128, 152, 1, 18, 32, 52,
          189, 238, 44, 52, 228, 153, 78, 3, 11,
          253, 252, 168, 165, 91, 172, 110, 34,
          30, 172, 203, 223, 102, 173, 232, 127,
          77, 55, 193, 186, 63, 222, 40, 1, 48, 1
      };*/
      auto stream_p = std::move(stream_res.value());
      ASSERT_FALSE(stream_p->isClosedForWrite());

      auto rw = std::make_shared<libp2p::basic::MessageReadWriterUvarint>(stream_p);
      rw->write(data, [](auto res) {

      });

/*      stream_p->write(
          request_buf,
          request_buf.size(),
          [request_buf, stream_p](auto &&write_res) {
            stream_p->close([stream_p{std::move(stream_p)}] (auto res) {
              std::vector<uint8_t> read_buf{};
              read_buf.resize(10);
              stream_p->read(read_buf, 10, [read_buf, stream_p](auto &&read_res) {
                FAIL() << "Read res: " << read_res.error().message();
                ASSERT_TRUE(read_res) << read_res.error().message();
              });
            });
          });*/

      //proto2::Message::
      /*auto msg = std::make_shared<api::v1::BlockRequest>();
      msg->set_fields(19);
      uint8_t s = 5;
      msg->set_number(&s, 1);
      msg->set_direction(api::v1::Direction::Descending);
      msg->set_max_blocks(6);*/

      //MessageReadWriterBigEndian
      //auto rw = std::make_shared<libp2p::basic::ProtobufMessageReadWriter>(std::make_shared<libp2p::basic::MessageReadWriterUvarint>(stream_p));
      /*auto rw = std::make_shared<libp2p::basic::ProtobufMessageReadWriter>(std::make_shared<libp2p::basic::MessageReadWriterUvarint>(stream_p));
      rw->write(*msg, [stream_p{std::move(stream_p)}](libp2p::outcome::result<size_t> res) {
        auto q = res.value();
        std::cerr << "Written: " << q << std::endl;
        //FAIL() << "Write res: " << res.error().message();
        ASSERT_TRUE(res) << res.error().message();

        stream_p->close([stream_p{std::move(stream_p)}] (auto res) {
          ASSERT_TRUE(res) << res.error().message();
          ASSERT_TRUE(stream_p->isClosedForWrite());
          ASSERT_FALSE(stream_p->isClosedForRead());
          //FAIL() << "Close res: " << res.error().message();

          libp2p::basic::ProtobufMessageReadWriter::ReadCallbackFunc<api::v1::BlockResponse> f =
              [](libp2p::outcome::result<api::v1::BlockResponse> res) {
                auto q = res.value();
                ASSERT_TRUE(res) << res.error().message();
              };

          auto rw = std::make_shared<libp2p::basic::ProtobufMessageReadWriter>(stream_p);
          rw->read(std::move(f));
        });
      });*/

      //{8, 19, 26, 1, 5, 40, 1, 48, 1}
      //{9, 8, 19, 26, 1, 5, 40, 1, 48, 1}
      //libp2p::outcome::result<api::v1::BlockResponse> res) {


      //identify::pb::Identify id;
      //rw->write()

/*      std::vector<uint8_t> read_buf{};
      read_buf.resize(10);
      stream_p->read(read_buf, 5, [read_buf, stream_p](auto &&read_res) {
        FAIL() << "Read res: " << read_res.error().message();

        ASSERT_TRUE(read_res) << read_res.error().message();
        FAIL() << "!!!!!!!!!!!!!!!!!!!";
      });*/

      //strcpy((char*)&request_buf[0], "Hello!!!!!");
      /*stream_p->write(
          request_buf,
          request_buf.size(),
          [request_buf, stream_p](auto &&write_res) {
            ASSERT_TRUE(write_res) << write_res.error().message();*/
/*            std::vector<uint8_t> read_buf{};
            read_buf.resize(10);
            stream_p->read(read_buf, 10, [read_buf, stream_p](auto &&read_res) {
              FAIL() << "Read res: " << read_res.error().message();

              ASSERT_TRUE(read_res) << read_res.error().message();
              FAIL() << common::Buffer(read_buf).toHex();
            });*/

      //ASSERT_TRUE(write_res) << write_res.error().message();
      //ASSERT_EQ(request_buf.size(), write_res.value());
      //});
    });
  });

  try {
    //ping->start();
    context->run();
  } catch (std::exception &e) {
    std::cout << e.what();
  }

  int p =0; ++p;
}
