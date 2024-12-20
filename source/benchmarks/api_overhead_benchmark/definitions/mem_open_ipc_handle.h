/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "framework/argument/basic_argument.h"
#include "framework/argument/enum/usm_memory_placement_argument.h"
#include "framework/test_case/test_case.h"

struct MemOpenIpcHandleArguments : TestCaseArgumentContainer {
  UsmMemoryPlacementArgument sourcePlacement;
  IntegerArgument AllocationsCount;

  MemOpenIpcHandleArguments()
      : sourcePlacement(*this, "src", "Placement of the source buffer"),
        AllocationsCount(
            *this, "AmountOfUsmAllocations",
            "Amount of USM allocations that are present in system") {}
};

struct MemOpenIpcHandle : TestCase<MemOpenIpcHandleArguments> {
  using TestCase<MemOpenIpcHandleArguments>::TestCase;

  std::string getTestCaseName() const override { return "MemOpenIpcHandle"; }

  std::string getHelp() const override {
    return "measures time spent in zeMemOpenIpcHandle on CPU.";
  }
};
