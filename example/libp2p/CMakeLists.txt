#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

add_executable(libp2p_server
    libp2p_server.cpp
    )

target_link_libraries(libp2p_server
    libp2p
    libp2p_basic_host
    libp2p_default_network
    libp2p_peer_repository
    libp2p_inmem_address_repository
    libp2p_inmem_key_repository
    libp2p_inmem_protocol_repository
    )
