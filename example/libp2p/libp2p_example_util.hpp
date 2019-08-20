/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_EXAMPLE_LIBP2P_LIBP2P_EXAMPLE_UTIL_HPP
#define KAGOME_EXAMPLE_LIBP2P_LIBP2P_EXAMPLE_UTIL_HPP

#include <vector>

std::string make_string(const std::vector<uint8_t> &buffer, size_t count) {
  auto begin = buffer.begin();
  auto end = begin + count;
  return std::string(begin, end);
}

#endif  // KAGOME_EXAMPLE_LIBP2P_LIBP2P_EXAMPLE_UTIL_HPP
