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

int main() {
  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useSecurityAdaptors<libp2p::security::Plaintext>(),
      libp2p::injector::useMuxerAdaptors<libp2p::muxer::Yamux>(),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::TcpTransport>());

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

  boost::asio::io_context context;

  context.post([host{std::move(host)}] {
    auto ma =
        libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/40009").value();
    host->listen(ma);
    host->start();
  });

  context.run();
}
