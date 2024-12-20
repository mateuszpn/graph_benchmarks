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

#include "framework/argument/compression_argument.h"
#include "framework/argument/enum/buffer_contents_argument.h"
#include "framework/argument/enum/embargo/stream_memory_embargo_type_argument.h"
#include "framework/test_case/test_case.h"

struct StreamMemoryImmediateEmbargoArguments : TestCaseArgumentContainer {
  StreamMemoryEmbargoTypeArgument type;
  ByteSizeArgument size;
  BooleanArgument useEvents;

  StreamMemoryImmediateEmbargoArguments()
      : type(*this, "type", "Memory streaming type"),
        size(*this, "size",
             "Size of the memory to stream. Must be divisible by datatype "
             "size."),
        useEvents(*this, "useEvents", CommonHelpMessage::useEvents()) {}
};

struct StreamMemoryImmediateEmbargo
    : TestCase<StreamMemoryImmediateEmbargoArguments> {
  using TestCase<StreamMemoryImmediateEmbargoArguments>::TestCase;

  std::string getTestCaseName() const override {
    return "StreamMemoryImmediateEmbargo";
  }

  std::string getHelp() const override {
    return "Streams memory inside of kernel in a fashion described by 'type' "
           "using immediate command list. "
           "Copy means one memory location is read from and the second one is "
           "written to. Triad means two "
           "buffers are read and one is written to. In read and write memory "
           "is only read or "
           "written to.";
  }
};
