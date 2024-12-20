/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2021-2022 Intel Corporation
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

#pragma once

#include "framework/argument/basic_argument.h"
#include "framework/test_case/test_case.h"

struct EmptyKernelEmbargoArguments : TestCaseArgumentContainer {
  PositiveIntegerArgument workgroupCount;
  PositiveIntegerArgument workgroupSize;

  EmptyKernelEmbargoArguments()
      : workgroupCount(*this, "wgc", "Workgroup count"),
        workgroupSize(*this, "wgs", "Workgroup size (aka local work size)") {}
};

struct EmptyKernelEmbargo : TestCase<EmptyKernelEmbargoArguments> {
  using TestCase<EmptyKernelEmbargoArguments>::TestCase;

  std::string getTestCaseName() const override { return "EmptyKernelEmbargo"; }

  std::string getHelp() const override {
    return "enqueues empty kernel and measures time to launch it and wait for "
           "it on CPU, thus "
           "measuring walker spawn time.";
  }
};
