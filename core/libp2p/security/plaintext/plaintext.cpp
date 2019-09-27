/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libp2p/security/plaintext/plaintext.hpp"

#include <functional>
#include <iostream>

#include "libp2p/peer/peer_id.hpp"
#include "libp2p/security/error.hpp"
#include "libp2p/security/plaintext/plaintext_connection.hpp"

#define PLAINTEXT_OUTCOME_TRY(name, res, conn, cb) \
  auto(name) = (res);                              \
  if ((name).has_error()) {                        \
    (void)(conn)->close();                         \
    cb((name).error());                            \
    return;                                        \
  }

OUTCOME_CPP_DEFINE_CATEGORY(libp2p::security, Plaintext::Error, e) {
  using E = libp2p::security::Plaintext::Error;
  switch (e) {
    case E::EXCHANGE_SEND_ERROR:
      return "Error occured while sending Exchange message to the peer";
    case E::EXCHANGE_RECEIVE_ERROR:
      return "Error occured while receiving Exchange message to the peer";
    case E::INVALID_PEER_ID:
      return "Received peer id doesn't match actual peer id";
    case E::EMPTY_PEER_ID:
      return "Peer Id isn't present in the remote peer multiaddr";
  }
  return "Unknown error";
}

namespace {
  /**
   * Close (\param conn) and report error in case of failure
   */
  void closeConnection(
      const std::shared_ptr<libp2p::connection::RawConnection> &conn) {
    if (auto close_res = conn->close(); !close_res) {
      std::cerr << "connection close attempt ended with error: "
                << close_res.error().message();
    }
  }
}  // namespace

namespace libp2p::security {

  Plaintext::Plaintext(
      std::shared_ptr<plaintext::ExchangeMessageMarshaller> marshaller,
      std::shared_ptr<peer::IdentityManager> idmgr,
      std::shared_ptr<crypto::marshaller::KeyMarshaller> key_marshaller)
      : marshaller_(std::move(marshaller)),
        idmgr_(std::move(idmgr)),
        key_marshaller_{std::move(key_marshaller)} {
    BOOST_ASSERT(marshaller_);
    BOOST_ASSERT(idmgr_);
    BOOST_ASSERT(key_marshaller_);
  }

  peer::Protocol Plaintext::getProtocolId() const {
    // TODO(akvinikym) 29.05.19: think about creating SecurityProtocolRegister
    return "/plaintext/2.0.0";
  }

  void Plaintext::secureInbound(
      std::shared_ptr<connection::RawConnection> inbound,
      SecConnCallbackFunc cb) {
    sendExchangeMsg(inbound, cb);
    receiveExchangeMsg(inbound, boost::none, cb);
  }

  void Plaintext::secureOutbound(
      std::shared_ptr<connection::RawConnection> outbound,
      const peer::PeerId &p,
      SecConnCallbackFunc cb) {
    sendExchangeMsg(outbound, cb);
    receiveExchangeMsg(outbound, p, cb);
  }

  void Plaintext::sendExchangeMsg(
      const std::shared_ptr<connection::RawConnection> &conn,
      SecConnCallbackFunc cb) const {
    PLAINTEXT_OUTCOME_TRY(out_msg_res,
                          marshaller_->marshal(plaintext::ExchangeMessage{
                              .pubkey = idmgr_->getKeyPair().publicKey,
                              .peer_id = idmgr_->getId()}),
                          conn,
                          cb)

    auto out_msg = out_msg_res.value();
    auto len = out_msg.size();

    std::vector<uint8_t> len_bytes = {static_cast<uint8_t>(len >> 24u),
                                      static_cast<uint8_t>(len >> 16u),
                                      static_cast<uint8_t>(len >> 8u),
                                      static_cast<uint8_t>(len)};

    conn->write(len_bytes, 4, [out_msg, conn, cb{std::move(cb)}](auto &&res) {
      if (res.has_error()) {
        closeConnection(conn);
        return cb(Error::EXCHANGE_SEND_ERROR);
      }
      std::cout << "Plaintext: wrote " << res.value() << " bytes\n";

      conn->write(out_msg, out_msg.size(), [cb{cb}, conn](auto &&res) {
        if (res.has_error()) {
          closeConnection(conn);
          return cb(Error::EXCHANGE_SEND_ERROR);
        }
        std::cout << "Plaintext: wrote " << res.value() << " bytes\n";
      });
    });
  }

  void Plaintext::receiveExchangeMsg(
      const std::shared_ptr<connection::RawConnection> &conn,
      const MaybePeerId &p,
      SecConnCallbackFunc cb) const {
    constexpr size_t kMaxMsgSize = 4;  // we read uint32_t first
    auto read_bytes = std::make_shared<std::vector<uint8_t>>(kMaxMsgSize);

    conn->read(
        *read_bytes,
        kMaxMsgSize,
        [self{shared_from_this()}, conn, p, cb{std::move(cb)}, read_bytes](
            auto &&r) {
          auto bytes_size = (static_cast<uint32_t>(read_bytes->at(0)) << 24u)
                            + (static_cast<uint32_t>(read_bytes->at(1)) << 16u)
                            + (static_cast<uint32_t>(read_bytes->at(2)) << 8u)
                            + read_bytes->at(3);

          auto received_bytes =
              std::make_shared<std::vector<uint8_t>>(bytes_size);
          std::cout << "Plaintext: bytes to be read: " << bytes_size << "\n";
          conn->read(*received_bytes,
                     received_bytes->size(),
                     [self, conn, p, cb, received_bytes](auto &&r) {
                       self->readCallback(conn, p, cb, received_bytes, r);
                     });
        });
  }

  void Plaintext::readCallback(
      std::shared_ptr<connection::RawConnection> conn,  // NOLINT
      const MaybePeerId &p,
      const SecConnCallbackFunc &cb,
      const std::shared_ptr<std::vector<uint8_t>> &read_bytes,
      outcome::result<size_t> read_call_res) const {
    PLAINTEXT_OUTCOME_TRY(r, read_call_res, conn, cb)
    size_t read_num = r.value();
    std::cout << "Plaintext: callback: read " << read_num << " bytes\n";

    PLAINTEXT_OUTCOME_TRY(
        in_exchange_msg, marshaller_->unmarshal(*read_bytes), conn, cb)
    auto &msg = in_exchange_msg.value().first;
    auto received_pid = msg.peer_id;
    auto pkey = msg.pubkey;

    // PeerId is derived from the Protobuf-serialized public key, not raw one
    auto derived_pid =
        peer::PeerId::fromPublicKey(in_exchange_msg.value().second);
    if (received_pid != derived_pid) {
      std::cerr << "Plaintext: received ID {" << received_pid.toBase58()
                << "} and derived from the public key {"
                << derived_pid.toBase58() << "} differ\n";
      closeConnection(conn);
      return cb(Error::INVALID_PEER_ID);
    }
    if (p.has_value()) {
      if (received_pid != p.value()) {
        closeConnection(conn);
        return cb(Error::INVALID_PEER_ID);
      }
    }

    cb(std::make_shared<connection::PlaintextConnection>(
        std::move(conn),
        idmgr_->getKeyPair().publicKey,
        std::move(pkey),
        key_marshaller_));
  }

}  // namespace libp2p::security
