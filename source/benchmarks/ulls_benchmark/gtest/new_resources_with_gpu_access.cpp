/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "definitions/new_resources_with_gpu_access.h"

#include "framework/test_case/register_test_case.h"
#include "framework/utility/common_gtest_args.h"
#include "framework/utility/memory_constants.h"

#include <gtest/gtest.h>

[[maybe_unused]] static const inline RegisterTestCase<NewResourcesWithGpuAccess>
    registerTestCase{};

class NewResourcesWithGpuAccessTest
    : public ::testing::TestWithParam<std::tuple<Api, size_t>> {};

TEST_P(NewResourcesWithGpuAccessTest, Test) {
  NewResourcesWithGpuAccessArguments args;
  args.api = std::get<0>(GetParam());
  args.size = std::get<1>(GetParam());

  NewResourcesWithGpuAccess test;
  test.run(args);
}

using namespace MemoryConstants;
INSTANTIATE_TEST_SUITE_P(
    NewResourcesWithGpuAccessTest, NewResourcesWithGpuAccessTest,
    ::testing::Combine(::CommonGtestArgs::allApis(),
                       ::testing::Values(kiloByte, 64 * kiloByte,
                                         512 * kiloByte, 1 * megaByte,
                                         16 * megaByte, 64 * megaByte,
                                         256 * megaByte, 1 * gigaByte)));
