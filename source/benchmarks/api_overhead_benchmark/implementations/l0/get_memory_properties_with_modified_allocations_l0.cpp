/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/l0/levelzero.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/file_helper.h"
#include "framework/utility/timer.h"

#include "definitions/get_memory_properties_with_modified_allocations.h"

#include <gtest/gtest.h>

static TestResult
run(const GetMemoryPropertiesWithModifiedAllocationsArguments &arguments,
    Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::Microseconds,
                                 MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  // Setup
  LevelZero levelzero;
  Timer timer;

  std::vector<void *> allocations;
  allocations.reserve(arguments.AllocationsCount);

  for (int64_t i = 0; i < arguments.AllocationsCount; i++) {
    const ze_device_mem_alloc_desc_t deviceAllocationDesc{
        ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    void *ptr = nullptr;
    ASSERT_ZE_RESULT_SUCCESS(
        zeMemAllocDevice(levelzero.context, &deviceAllocationDesc,
                         sizeof(int32_t), 4u, levelzero.device, &ptr));
    allocations.push_back(ptr);
  }

  auto modifyAllocations = [&]() {
    for (int64_t index = 0; index < arguments.AllocationsCount; index++) {
      // Modify alternate allocations
      if (index & 1) {
        ASSERT_ZE_RESULT_SUCCESS(
            zeMemFree(levelzero.context, allocations[index]));
        const ze_device_mem_alloc_desc_t deviceAllocationDesc{
            ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        void *ptr = nullptr;
        ASSERT_ZE_RESULT_SUCCESS(
            zeMemAllocDevice(levelzero.context, &deviceAllocationDesc,
                             sizeof(uint64_t), 8u, levelzero.device, &ptr));
        allocations[index] = ptr;
      }
    }
    return TestResult::Success;
  };

  // Warmup
  ze_memory_allocation_properties_t properties{};
  for (int64_t i = 0; i < arguments.AllocationsCount; i++) {
    ASSERT_ZE_RESULT_SUCCESS(zeMemGetAllocProperties(
        levelzero.context, allocations[i], &properties, nullptr));
  }

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    timer.measureStart();
    for (int64_t j = 0; j < arguments.AllocationsCount; ++j) {
      ASSERT_ZE_RESULT_SUCCESS(zeMemGetAllocProperties(
          levelzero.context, allocations[j], &properties, nullptr));
    }
    timer.measureEnd();

    auto status = modifyAllocations();
    if (status != TestResult::Success) {
      return status;
    }

    statistics.pushValue(timer.get() / arguments.AllocationsCount,
                         typeSelector.getUnit(), typeSelector.getType());
  }

  // Cleanup
  for (int64_t i = 0; i < arguments.AllocationsCount; i++) {
    ASSERT_ZE_RESULT_SUCCESS(zeMemFree(levelzero.context, allocations[i]));
  }

  return TestResult::Success;
}

static RegisterTestCaseImplementation<
    GetMemoryPropertiesWithModifiedAllocations>
    registerTestCase(run, Api::L0);
