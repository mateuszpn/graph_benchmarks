/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "definitions/sin_kernel_graph.h"

#include "framework/test_case/register_test_case.h"
#include "framework/utility/common_gtest_args.h"

#include <gtest/gtest.h>

[[maybe_unused]] static const inline RegisterTestCase<SinKernelGraph>
    registerTestCase{};

class SinKernelGraphTest
    : public ::testing::TestWithParam<std::tuple<Api, std::size_t, bool>> {};

TEST_P(SinKernelGraphTest, Test) {
  SinKernelGraphArguments args{};
  args.api = std::get<0>(GetParam());
  args.numKernels = std::get<1>(GetParam());
  args.withGraphs = std::get<2>(GetParam());

  SinKernelGraph test;
  test.run(args);
}

INSTANTIATE_TEST_SUITE_P(
    SinKernelGraphTest, SinKernelGraphTest,
    ::testing::Combine(::testing::Values(Api::SYCL, Api::UR, Api::L0),
                       ::testing::Values(3 /* 20, 50, 100, 500 */),
                       ::testing::Values(false, true)));