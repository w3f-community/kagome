/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <iostream>

#include "libp2p/host/host.hpp"
#include "libp2p/injector/host_injector.hpp"
#include "libp2p/muxer/yamux.hpp"
#include "libp2p/security/plaintext.hpp"
#include "libp2p/transport/tcp.hpp"

using std::chrono_literals::operator""s;  // for seconds

using MuxedConnectionConfig = libp2p::muxer::MuxedConnectionConfig;

namespace {
  std::vector<uint8_t> operator"" _v(const char *c, size_t s) {
    std::vector<uint8_t> chars(c, c + s);
    return chars;
  }

  std::string make_string(const std::vector<uint8_t> &buffer, size_t count) {
    auto begin = buffer.begin();
    auto end = begin + count;
    return std::string (begin, end);
  }

}  // namespace

int main() {
  using context_t = boost::asio::io_context;

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
        auto read_buffer = std::make_shared<std::vector<uint8_t>>(100, 0);
        stream->readSome(
            *read_buffer,
            read_buffer->size(),
            [read_buffer, stream](outcome::result<size_t> read_bytes) mutable {
              if (read_bytes) {
                auto s = make_string(*read_buffer, read_bytes.value());
                std::cout << "Server read: " << s << std::endl;
                auto write_buffer = std::make_shared<std::vector<uint8_t>>();
                *write_buffer = "hello there"_v;

                stream->write(
                    *write_buffer,
                    write_buffer->size(),
                    [write_buffer](
                        outcome::result<size_t> written_bytes) mutable {
                      if (written_bytes) {
                        std::cout << written_bytes.value() << " sent"
                                  << std::endl;
                      }
                    });
              }
            });
      });

  auto context = injector.create<std::shared_ptr<context_t>>();

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
