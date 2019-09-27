/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libp2p/muxer/yamux/yamux_frame.hpp"

#include <arpa/inet.h>

namespace libp2p::connection {
  using kagome::common::Buffer;

  Buffer YamuxFrame::frameBytes(uint8_t version,
                                FrameType type,
                                Flag flag,
                                uint32_t stream_id,
                                uint32_t length,
                                gsl::span<const uint8_t> data) {
    return Buffer{}
        .putUint8(version)
        .putUint8(static_cast<uint8_t>(type))
        .putUint16BE(static_cast<uint16_t>(flag))
        .putUint32BE(stream_id)
        .putUint32BE(length)
        .put(data);
  }

  bool YamuxFrame::flagIsSet(Flag flag) const {
    return static_cast<uint16_t>(flag) & flags;
  }

  kagome::common::Buffer newStreamMsg(YamuxFrame::StreamId stream_id) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::DATA,
                                  YamuxFrame::Flag::SYN,
                                  stream_id,
                                  0);
  }

  kagome::common::Buffer ackStreamMsg(YamuxFrame::StreamId stream_id) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::DATA,
                                  YamuxFrame::Flag::ACK,
                                  stream_id,
                                  0);
  }

  kagome::common::Buffer closeStreamMsg(YamuxFrame::StreamId stream_id) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::DATA,
                                  YamuxFrame::Flag::FIN,
                                  stream_id,
                                  0);
  }

  kagome::common::Buffer resetStreamMsg(YamuxFrame::StreamId stream_id) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::DATA,
                                  YamuxFrame::Flag::RST,
                                  stream_id,
                                  0);
  }

  kagome::common::Buffer pingOutMsg(uint32_t value) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::PING,
                                  YamuxFrame::Flag::SYN,
                                  0,
                                  value);
  }

  kagome::common::Buffer pingResponseMsg(uint32_t value) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::PING,
                                  YamuxFrame::Flag::ACK,
                                  0,
                                  value);
  }

  kagome::common::Buffer dataMsg(YamuxFrame::StreamId stream_id,
                                 gsl::span<const uint8_t> data) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::DATA,
                                  YamuxFrame::Flag::NONE,
                                  stream_id,
                                  static_cast<uint32_t>(data.size()),
                                  data);
  }

  kagome::common::Buffer goAwayMsg(YamuxFrame::GoAwayError error) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::GO_AWAY,
                                  YamuxFrame::Flag::NONE,
                                  0,
                                  static_cast<uint32_t>(error));
  }

  kagome::common::Buffer windowUpdateMsg(YamuxFrame::StreamId stream_id,
                                         uint32_t window_delta) {
    return YamuxFrame::frameBytes(YamuxFrame::kDefaultVersion,
                                  YamuxFrame::FrameType::WINDOW_UPDATE,
                                  YamuxFrame::Flag::NONE,
                                  stream_id,
                                  window_delta);
  }

  boost::optional<YamuxFrame> parseFrame(gsl::span<const uint8_t> frame_bytes) {
    if (frame_bytes.size() < YamuxFrame::kHeaderLength) {
      return {};
    }

    YamuxFrame frame{};

    frame.version = frame_bytes[0];

    switch (frame_bytes[1]) {
      case 0:
        frame.type = YamuxFrame::FrameType::DATA;
        break;
      case 1:
        frame.type = YamuxFrame::FrameType::WINDOW_UPDATE;
        break;
      case 2:
        frame.type = YamuxFrame::FrameType::PING;
        break;
      case 3:
        frame.type = YamuxFrame::FrameType::GO_AWAY;
        break;
      default:
        return {};
    }

    uint16_t flags;
    memcpy(&flags, &frame_bytes[2], sizeof(flags));
    frame.flags = ntohs(flags);

    uint32_t temp32;
    memcpy(&temp32, &frame_bytes[4], sizeof(temp32));
    frame.stream_id = ntohl(temp32);

    memcpy(&temp32, &frame_bytes[8], sizeof(temp32));
    frame.length = ntohl(temp32);

    const auto &data_begin = frame_bytes.begin() + YamuxFrame::kHeaderLength;
    if (data_begin != frame_bytes.end()) {
      frame.data = kagome::common::Buffer{
          std::vector<uint8_t>(data_begin, frame_bytes.end())};
    }

    return frame;
  }
}  // namespace libp2p::connection
