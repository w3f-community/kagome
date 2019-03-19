/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "common/result.hpp"
#include "common/scale/basic_stream.hpp"
#include "common/scale/optional.hpp"

using namespace kagome;
using namespace kagome::common;
using namespace common::scale;

/**
 * @given variety of optional values
 * @when encodeOptional function is applied
 * @then expected result obtained
 */
TEST(Scale, encodeOptional) {
  // most simple case
  {
    Buffer out;
    auto res =
        optional::encodeOptional(std::optional<uint8_t>{std::nullopt}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{0}));
  }

  // encode existing uint8_t
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<uint8_t>{1}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{1, 1}));
  }

  // encode negative int8_t
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<int8_t>{-1}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{1, 255}));
  }

  // encode non-existing uint16_t
  {
    Buffer out;
    auto res =
        optional::encodeOptional(std::optional<uint16_t>{std::nullopt}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{0}));
  }

  // encode existing uint16_t
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<uint16_t>{511}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{1, 255, 1}));
  }

  // encode existing uint32_t
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<uint32_t>{67305985}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{1, 1, 2, 3, 4}));
  }
}

/**
 * @given byte stream containing series of encoded optional values
 * @when decodeOptional function sequencially applied
 * @then expected values obtained
 */
TEST(Scale, decodeOptional) {
  // clang-format off
    auto bytes = ByteArray{
            0,              // first value
            1, 1,           // second value
            1, 255,         // third value
            0,              // fourth value
            1, 255, 1,      // fifth value
            1, 1, 2, 3, 4}; // sixth value
  // clang-format on

  auto stream = BasicStream{bytes};

  // decode nullopt uint8_t
  optional::decodeOptional<uint8_t>(stream).match(
      [](const expected::Value<std::optional<uint8_t>> &v) {
        ASSERT_EQ(v.value, std::nullopt);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode optional uint8_t
  optional::decodeOptional<uint8_t>(stream).match(
      [](const expected::Value<std::optional<uint8_t>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ((*v.value), 1);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode optional negative int8_t
  optional::decodeOptional<int8_t>(stream).match(
      [](const expected::Value<std::optional<int8_t>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ((*v.value), -1);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode nullopt uint16_t
  // it requires 1 zero byte just like any other nullopt
  optional::decodeOptional<uint16_t>(stream).match(
      [](const expected::Value<std::optional<uint16_t>> &v) {
        ASSERT_EQ(v.value.has_value(), false);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode optional uint16_t
  optional::decodeOptional<uint16_t>(stream).match(
      [](const expected::Value<std::optional<uint16_t>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ((*v.value), 511);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode optional uint32_t
  optional::decodeOptional<uint32_t>(stream).match(
      [](const expected::Value<std::optional<uint32_t>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ((*v.value), 67305985);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });
}

/**
 * Optional bool is a special case of optinal values in SCALE
 * Optional bools are encoded using only 1 byte
 * 0 means no value, 1 means false, 2 means true
 */

/**
 * @given stream containing series of encoded optionalBool values
 * @when decodeOptional<bool> function is applied
 * @then expected values obtained
 */
TEST(Scale, decodeoptionalBool) {
  auto bytes = ByteArray{0, 1, 2, 3};
  auto stream = BasicStream{bytes};

  // decode none
  optional::decodeOptional<bool>(stream).match(
      [](const expected::Value<std::optional<bool>> &v) {
        ASSERT_EQ(v.value.has_value(), false);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode false
  optional::decodeOptional<bool>(stream).match(
      [](const expected::Value<std::optional<bool>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ(*v.value, false);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // decode true
  optional::decodeOptional<bool>(stream).match(
      [](const expected::Value<std::optional<bool>> &v) {
        ASSERT_EQ(v.value.has_value(), true);
        ASSERT_EQ(*v.value, true);
      },
      [](const expected::Error<DecodeError> &) { FAIL(); });

  // dedode error unexpected value
  optional::decodeOptional<bool>(stream).match(
      [](const expected::Value<std::optional<bool>> &v) { FAIL(); },
      [](const expected::Error<DecodeError> &e) {
        ASSERT_EQ(e.error, DecodeError::kUnexpectedValue);
      });

  // not enough data
  optional::decodeOptional<bool>(stream).match(
      [](const expected::Value<std::optional<bool>> &v) { FAIL(); },
      [](const expected::Error<DecodeError> &e) {
        ASSERT_EQ(e.error, DecodeError::kNotEnoughData);
      });
}

/**
 * @given series of all possible values of optional bool value
 * @when encodeOptional<bool> is applied
 * @then expected values obtained
 */
TEST(Scale, encodeOptionalBool) {
  // encode none
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<bool>{std::nullopt}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{0}));
  }
  // encode false
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<bool>{false}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{1}));
  }
  // encode true
  {
    Buffer out;
    auto res = optional::encodeOptional(std::optional<bool>{true}, out);
    ASSERT_EQ(res, true);
    ASSERT_EQ(out.toVector(), (ByteArray{2}));
  }
}