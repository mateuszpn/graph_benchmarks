/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "definitions/execute_command_list_with_indirect_access.h"

#include "framework/test_case/register_test_case.h"

#include <gtest/gtest.h>

[[maybe_unused]] static const inline RegisterTestCase<
    ExecuteCommandListWithIndirectAccess>
    registerTestCase{};

class ExecuteCommandListTestWithIndirectAccessTest
    : public ::testing::TestWithParam<std::tuple<size_t, bool>> {};

TEST_P(ExecuteCommandListTestWithIndirectAccessTest, Test) {
  ExecuteCommandListWithIndirectAccessArguments args{};
  args.api = Api::L0;
  args.IndirectAllocationsAmount = std::get<0>(GetParam());
  args.AllocateMemory = std::get<1>(GetParam());

  ExecuteCommandListWithIndirectAccess test;
  test.run(args);
}

INSTANTIATE_TEST_SUITE_P(ExecuteCommandListTestWithIndirectAccessTest,
                         ExecuteCommandListTestWithIndirectAccessTest,
                         ::testing::Combine(::testing::Values(10u, 100u, 1000u),
                                            ::testing::Values(false, true)));
