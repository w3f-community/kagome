#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

add_subdirectory(multi)
add_subdirectory(transport)
add_subdirectory(crypto)
add_subdirectory(peer)
add_subdirectory(network)
add_subdirectory(protocol_muxer)
add_subdirectory(muxer)
add_subdirectory(security)
add_subdirectory(connection)
add_subdirectory(protocol)
add_subdirectory(basic)
add_subdirectory(host)

add_library(host_builder
    host_builder.hpp
    host_builder.cpp
    )
target_link_libraries(host_builder
    Boost::boost
    libp2p_basic_host
    libp2p_default_network
    libp2p_inmem_address_repository
    libp2p_inmem_key_repository
    libp2p_inmem_protocol_repository
    libp2p_peer_repository
    )

add_library(libp2p
    libp2p.cpp
    )
target_link_libraries(libp2p INTERFACE
    libp2p_default_network
    )
