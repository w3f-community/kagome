/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_NETWORK_TYPES_ROLES_HPP
#define KAGOME_CORE_NETWORK_TYPES_ROLES_HPP

namespace kagome::network {

  union Roles {
    struct {
      /**
       * Full node, does not participate in consensus.
       */
      uint8_t full : 1;

      /**
       * Light client node.
       */
      uint8_t light : 1;

      /**
       * Act as an authority
       */
      uint8_t authority : 1;

    } flags;
    uint8_t value;
  };

}  // namespace kagome::network

#endif  // KAGOME_CORE_NETWORK_TYPES_ROLES_HPP
