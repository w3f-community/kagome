/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include "libp2p/host/host.hpp"
#include "libp2p/injector/host_injector.hpp"
#include "libp2p/muxer/yamux.hpp"
#include "libp2p/security/plaintext.hpp"
#include "libp2p/transport/tcp.hpp"

#include <chrono>

using std::chrono_literals::operator""s;  // for seconds

using MuxedConnectionConfig = libp2p::muxer::MuxedConnectionConfig;

int main() {
  using context_t = boost::asio::io_context;
  //  auto injector = libp2p::injector::makeHostInjector(
  //      libp2p::injector::useSecurityAdaptors<libp2p::security::Plaintext>(),
  //      libp2p::injector::useMuxerAdaptors<libp2p::muxer::Yamux>(),
  //      libp2p::injector::useTransportAdaptors<
  //          libp2p::transport::TcpTransport>());

  // make injector use the same io_context and same MuxedConnectionConfig
  // instances every time he needs it to resolve dependency
  auto injector = libp2p::injector::makeHostInjector(
      boost::di::bind<context_t>.to<context_t>().in(boost::di::singleton),
      boost::di::bind<MuxedConnectionConfig>.to(MuxedConnectionConfig()));

  auto host = injector.create<std::shared_ptr<libp2p::Host>>();
  std::cout << "Server started" << std::endl;
  host->setProtocolHandler(
      "chat/1.0.0",
      [](const std::shared_ptr<libp2p::connection::Stream> &stream) {
        auto rcvd_data_msg = std::make_shared<std::vector<uint8_t>>(100, 0);
        stream->readSome(
            *rcvd_data_msg,
            rcvd_data_msg->size(),
            [rcvd_data_msg](outcome::result<size_t> read_bytes) {
              if (read_bytes) {
                std::string s(reinterpret_cast<char const *>(  // NOLINT
                    rcvd_data_msg->data()));
                std::cout << "Server read: " << s << std::endl;
              }
            });
      });

  auto context = injector.create<std::shared_ptr<context_t>>();
  std::cout << muxconfig.maximum_window_size << " " << muxconfig.maximum_streams
            << std::endl;

  context->post([host{std::move(host)}] {
    auto ma =
        libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/40009").value();
    auto res = host->listen(ma);
    if (!res) {
      std::cerr << res.error().message();
    }

    host->start();
  });

  context->run_for(2s);  // run for 2 seconds
}
