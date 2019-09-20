/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libp2p/security/plaintext/plaintext.hpp"

#include <functional>

#include "libp2p/peer/peer_id.hpp"
#include "libp2p/security/error.hpp"
#include "libp2p/security/plaintext/plaintext_connection.hpp"

#define PLAINTEXT_OUTCOME_TRY(name, res, conn, cb) \
  auto name = (res);                               \
  if (name.has_error()) {                          \
    conn->close();                                 \
    cb(name.error());                              \
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

namespace libp2p::security {

  Plaintext::Plaintext(
      std::shared_ptr<plaintext::ExchangeMessageMarshaller> marshaller,
      std::shared_ptr<peer::IdentityManager> idmgr)
      : marshaller_(std::move(marshaller)), idmgr_(std::move(idmgr)) {
    BOOST_ASSERT(marshaller_);
    BOOST_ASSERT(idmgr_);
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
                          cb);

    auto out_msg = out_msg_res.value();
    auto len = out_msg.size();

    std::vector<uint8_t> len_bytes = {static_cast<uint8_t>(len >> 24u),
                                      static_cast<uint8_t>(len >> 16u),
                                      static_cast<uint8_t>(len >> 8u),
                                      static_cast<uint8_t>(len)};

    conn->write(len_bytes, 4, [out_msg, conn, cb{std::move(cb)}](auto &&res) {
      if (res.has_error()) {
        conn->close();
        cb(Error::EXCHANGE_SEND_ERROR);
      }

      conn->write(
          out_msg, out_msg.size(), [cb{std::move(cb)}, conn](auto &&res) {
            if (res.has_error()) {
              conn->close();
              cb(Error::EXCHANGE_SEND_ERROR);
            }
          });
    });
  }

  void Plaintext::receiveExchangeMsg(
      const std::shared_ptr<connection::RawConnection> &conn,
      const MaybePeerId &p,
      SecConnCallbackFunc cb) const {
    constexpr size_t kMaxMsgSize = 4;  // we read uint32_t first
    auto read_bytes = std::make_shared<std::vector<uint8_t>>(kMaxMsgSize);

    conn->readSome(
        *read_bytes,
        kMaxMsgSize,
        [self{shared_from_this()}, conn, p, cb{std::move(cb)}, read_bytes](
            auto &&r) {
          auto bytes_size =
              (static_cast<uint32_t>(read_bytes->at(0)) << 24u) +
                  (static_cast<uint32_t>(read_bytes->at(1)) << 16u) +
                      (static_cast<uint32_t>(read_bytes->at(2)) << 8u) +
              read_bytes->at(3);

          auto received_bytes = std::make_shared<std::vector<uint8_t>>(10000);
          conn->readSome(*received_bytes,
                         received_bytes->size(),
                         [self, conn, p, cb, received_bytes](auto &&r) {
                           self->readCallback(conn, p, cb, received_bytes, r);
                         });
        });

    conn->readSome(
        *read_bytes,
        kMaxMsgSize,
        [self{shared_from_this()}, conn, p, cb{std::move(cb)}, read_bytes](
            auto &&r) { self->readCallback(conn, p, cb, read_bytes, r); });
  }
  void Plaintext::readCallback(
      std::shared_ptr<connection::RawConnection> conn,  // NOLINT
      const MaybePeerId &p,
      const SecConnCallbackFunc &cb,
      const std::shared_ptr<std::vector<uint8_t>> &read_bytes,
      outcome::result<size_t> read_call_res) const {
    PLAINTEXT_OUTCOME_TRY(r, read_call_res, conn, cb);
    PLAINTEXT_OUTCOME_TRY(
        in_exchange_msg, marshaller_->unmarshal(*read_bytes), conn, cb);
    auto received_pid = in_exchange_msg.value().peer_id;
    auto pkey = in_exchange_msg.value().pubkey;
    auto derived_pid = peer::PeerId::fromPublicKey(pkey.data);
    if (received_pid != derived_pid) {
      conn->close();
      cb(Error::INVALID_PEER_ID);
    }
    if (p.has_value()) {
      if (received_pid != p.value()) {
        conn->close();
        cb(Error::INVALID_PEER_ID);
      }
    }

    cb(std::make_shared<connection::PlaintextConnection>(
        std::move(conn), idmgr_->getKeyPair().publicKey, std::move(pkey)));
  }

}  // namespace libp2p::security
