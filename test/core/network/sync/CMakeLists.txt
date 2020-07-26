#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

addtest(sync_test
    sync_test.cpp
    )
target_link_libraries(sync_test
    syncing_node_application
    app_config_impl
    )
