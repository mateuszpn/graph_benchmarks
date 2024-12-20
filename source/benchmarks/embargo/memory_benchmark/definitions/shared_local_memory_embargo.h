/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2022 Intel Corporation
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

#include "framework/argument/compression_argument.h"
#include "framework/argument/enum/buffer_contents_argument.h"
#include "framework/argument/enum/embargo/stream_memory_embargo_type_argument.h"
#include "framework/test_case/test_case.h"

struct SharedLocalMemoryEmbargoArguments : TestCaseArgumentContainer {
  ByteSizeArgument slmSize;
  BooleanArgument useEvents;

  SharedLocalMemoryEmbargoArguments()
      : slmSize(*this, "slmSize", "Size of the local memory per thread group."),
        useEvents(*this, "useEvents", CommonHelpMessage::useEvents()) {}
};

struct SharedLocalMemoryEmbargo : TestCase<SharedLocalMemoryEmbargoArguments> {
  using TestCase<SharedLocalMemoryEmbargoArguments>::TestCase;

  std::string getTestCaseName() const override {
    return "SharedLocalMemoryEmbargo";
  }

  std::string getHelp() const override {
    return "Writes to SLM, then summing local memory and writes its value to a "
           "buffer. Uses different sizes of SLM to measure throughput.";
  }
};
