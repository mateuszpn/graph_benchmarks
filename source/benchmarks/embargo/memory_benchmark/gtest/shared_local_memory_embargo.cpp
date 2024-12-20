/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they were
 * provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit this
 * software or the related documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 */

#include "definitions/shared_local_memory_embargo.h"

#include "framework/test_case/register_test_case.h"
#include "framework/utility/common_gtest_args.h"
#include "framework/utility/memory_constants.h"

#include <gtest/gtest.h>

[[maybe_unused]] static const inline RegisterTestCase<SharedLocalMemoryEmbargo>
    registerTestCase{};

class SharedLocalMemoryEmbargoTest
    : public ::testing::TestWithParam<std::tuple<Api, size_t, bool>> {};

TEST_P(SharedLocalMemoryEmbargoTest, Test) {
  SharedLocalMemoryEmbargoArguments args;
  args.api = std::get<0>(GetParam());
  args.slmSize = std::get<1>(GetParam());
  args.useEvents = std::get<2>(GetParam());

  SharedLocalMemoryEmbargo test;
  test.run(args);
}

using namespace MemoryConstants;
INSTANTIATE_TEST_SUITE_P(
    SharedLocalMemoryEmbargoTest, SharedLocalMemoryEmbargoTest,
    ::testing::Combine(::CommonGtestArgs::allApis(),
                       ::testing::Values(1 * kiloByte, 2 * kiloByte,
                                         4 * kiloByte, 8 * kiloByte,
                                         16 * kiloByte, 32 * kiloByte,
                                         64 * kiloByte),
                       ::testing::Values(false, true)));
