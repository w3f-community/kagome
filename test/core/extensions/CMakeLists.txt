#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

addtest(crypto_extension_test
    crypto_extension_test.cpp
    )
target_link_libraries(crypto_extension_test
    crypto_extension
    )

addtest(io_extension_test
    io_extension_test.cpp
    )
target_link_libraries(io_extension_test
    io_extension
    )

addtest(memory_extension_test memory_extension_test)
target_link_libraries(memory_extension_test
    memory_extension
    )

addtest(misc_extension_test
    misc_extension_test.cpp
    )
target_link_libraries(misc_extension_test
    misc_extension
    )

addtest(storage_extension_test
    storage_extension_test.cpp
    )
target_link_libraries(storage_extension_test
    storage_extension
    dummy_error
    )
