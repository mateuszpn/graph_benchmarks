/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/benchmark_info.h"

#include "framework/utility/execute_at_app_init.h"

EXECUTE_AT_APP_INIT {
  const std::string name = "memory_benchmark";
  const std::string description = "Memory Benchmark is a set of tests aimed at "
                                  "measuring bandwidth of memory transfers.";
  const int testCaseColumnWidth = 126;
  BenchmarkInfo::initialize(name, description, testCaseColumnWidth);
};
