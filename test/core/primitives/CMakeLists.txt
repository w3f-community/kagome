#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

addtest(primitives_codec_test
    primitives_codec_test.cpp
    )

target_link_libraries(primitives_codec_test
    buffer
    outcome
    primitives
    scale
    testutil_primitives_generator
    Boost::boost
    )
