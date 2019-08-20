/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <iostream>

#include "../../test/testutil/literals.hpp"
#include "libp2p/host/host.hpp"
#include "libp2p/injector/host_injector.hpp"
#include "libp2p/muxer/yamux.hpp"
#include "libp2p/protocol/echo.hpp"
#include "libp2p/security/plaintext.hpp"
#include "libp2p/transport/tcp.hpp"
#include "libp2p_example_util.hpp"

using std::chrono_literals::operator""s;  // for seconds

int main() {
  auto injector = libp2p::injector::makeHostInjector();

  std::shared_ptr<libp2p::Host> host =
      injector.create<std::shared_ptr<libp2p::Host>>();

  libp2p::protocol::Echo echo;

  host->setProtocolHandler(
      "/echo/1.0.0",
      [](const std::shared_ptr<libp2p::connection::Stream> &stream) {
        auto read_buffer = std::make_shared<std::vector<uint8_t>>(100, 0);
        stream->readSome(
            *read_buffer,
            read_buffer->size(),
            [stream, read_buffer](auto read_bytes) {
              std::cout << make_string(*read_buffer, read_bytes.value());

              stream->writeSome(
                  *read_buffer, read_buffer->size(), [read_buffer](auto) {});
            });
      });

  using context_t = boost::asio::io_context;
  auto context = injector.create<std::shared_ptr<context_t>>();

  context->post([host{std::move(host)}] {
    auto ma = "/ip4/127.0.0.1/tcp/40010"_multiaddr;
    auto res = host->listen(ma);
    if (!res) {
      std::cerr << res.error().message();
    }

    host->start();
    std::cout << "Server started. Peer id: ";
    std::cout << host->getPeerInfo().id.toBase58() << std::endl;
  });

  context->run();  // run for 2 seconds
}
