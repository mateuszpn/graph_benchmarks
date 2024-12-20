/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2021-2023 Intel Corporation
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

#include "definitions/empty_kernel_embargo.h"

#include "framework/test_case/register_test_case.h"
#include "framework/utility/common_gtest_args.h"

#include <gtest/gtest.h>

[[maybe_unused]] static const inline RegisterTestCase<EmptyKernelEmbargo>
    registerTestCase{};

class EmptyKernelEmbargoSubmissionTest
    : public ::testing::TestWithParam<std::tuple<Api, size_t, size_t>> {};

TEST_P(EmptyKernelEmbargoSubmissionTest, Test) {
  EmptyKernelEmbargoArguments args;
  args.api = std::get<0>(GetParam());
  args.workgroupCount = std::get<1>(GetParam());
  args.workgroupSize = std::get<2>(GetParam());

  EmptyKernelEmbargo test;
  test.run(args);
}

INSTANTIATE_TEST_SUITE_P(
    EmptyKernelEmbargoSubmissionTest, EmptyKernelEmbargoSubmissionTest,
    ::testing::Combine(::CommonGtestArgs::allApis(),
                       ::CommonGtestArgs::workgroupCount(),
                       ::CommonGtestArgs::workgroupSize()));
