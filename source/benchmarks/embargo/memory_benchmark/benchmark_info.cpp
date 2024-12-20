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

#include "framework/benchmark_info.h"

#include "framework/utility/execute_at_app_init.h"

EXECUTE_AT_APP_INIT {
  const std::string name = "memory_benchmark_embargo";
  const std::string description = "Memory Benchmark is a set of tests aimed at "
                                  "measuring bandwidth of memory transfers.";
  const int testCaseColumnWidth = 126;
  BenchmarkInfo::initialize(name, description, testCaseColumnWidth);
};
