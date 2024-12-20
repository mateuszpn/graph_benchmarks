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

#include "framework/l0/levelzero.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/file_helper.h"
#include "framework/utility/memory_constants.h"
#include "framework/utility/timer.h"

#include "definitions/shared_local_memory_embargo.h"

#include <gtest/gtest.h>

using namespace MemoryConstants;

static TestResult run(const SharedLocalMemoryEmbargoArguments &arguments,
                      Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::GigabytesPerSecond,
                                 arguments.useEvents ? MeasurementType::Gpu
                                                     : MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  // Setup
  QueueProperties queueProperties = QueueProperties::create();
  ContextProperties contextProperties = ContextProperties::create();
  ExtensionProperties extensionProperties = ExtensionProperties::create();

  LevelZero levelzero(queueProperties, contextProperties, extensionProperties);
  if (levelzero.commandQueue == nullptr) {
    return TestResult::DeviceNotCapable;
  }

  size_t maxSlmSize =
      levelzero.getDeviceComputeProperties().maxSharedLocalMemory;
  if (arguments.slmSize > maxSlmSize) {
    return TestResult::DeviceNotCapable;
  }

  Timer timer;

  const size_t fillValue = 0;
  const size_t bufferSize = 1024 * kiloByte;
  const uint32_t gws = static_cast<uint32_t>(bufferSize / 4);
  const uint64_t timerResolution =
      levelzero.getTimerResolution(levelzero.device);

  // Create module
  const char *kernelFile = "shared_local_memory.spv";
  auto spirvModule = FileHelper::loadBinaryFile(kernelFile);
  if (spirvModule.size() == 0) {
    return TestResult::KernelNotFound;
  }
  ze_module_handle_t module;
  ze_kernel_handle_t kernel;
  ze_module_desc_t moduleDesc{ZE_STRUCTURE_TYPE_MODULE_DESC};
  moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
  moduleDesc.pInputModule =
      reinterpret_cast<const uint8_t *>(spirvModule.data());
  moduleDesc.inputSize = spirvModule.size();
  ASSERT_ZE_RESULT_SUCCESS(zeModuleCreate(levelzero.context, levelzero.device,
                                          &moduleDesc, &module, nullptr));

  // Create buffer
  void *buffer = nullptr;
  const ze_device_mem_alloc_desc_t deviceAllocationDesc{
      ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
  ASSERT_ZE_RESULT_SUCCESS(zeMemAllocDevice(levelzero.context,
                                            &deviceAllocationDesc, bufferSize,
                                            0, levelzero.device, &buffer));

  // Create kernel
  ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
  kernelDesc.pKernelName = "slm_alloc_size";
  ASSERT_ZE_RESULT_SUCCESS(zeKernelCreate(module, &kernelDesc, &kernel));

  const uint32_t groupSizeX = 256;

  // Configure kernel group size
  ASSERT_ZE_RESULT_SUCCESS(zeKernelSetGroupSize(kernel, groupSizeX, 1u, 1u));
  const ze_group_count_t dispatchTraits{gws / groupSizeX, 1u, 1u};

  ze_command_list_handle_t cmdList;
  ze_command_list_desc_t cmdListDesc{};
  cmdListDesc.commandQueueGroupOrdinal = levelzero.commandQueueDesc.ordinal;
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListCreate(
      levelzero.context, levelzero.device, &cmdListDesc, &cmdList));

  // Create event
  ze_event_pool_handle_t eventPool{};
  ze_event_handle_t event{};
  if (arguments.useEvents) {
    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags =
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    ASSERT_ZE_RESULT_SUCCESS(
        zeEventPoolCreate(levelzero.context, &eventPoolDesc, 1,
                          &levelzero.commandQueueDevice, &eventPool));
    ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    ASSERT_ZE_RESULT_SUCCESS(zeEventCreate(eventPool, &eventDesc, &event));
  }
  // Enqueue filling of the buffer and set kernel arguments
  const uint32_t doWrite = 0;
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryFill(
      cmdList, buffer, &fillValue, sizeof(fillValue), bufferSize, nullptr, 0,
      nullptr));
  ASSERT_ZE_RESULT_SUCCESS(
      zeKernelSetArgumentValue(kernel, 0, sizeof(buffer), &buffer));
  ASSERT_ZE_RESULT_SUCCESS(
      zeKernelSetArgumentValue(kernel, 1, arguments.slmSize, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(
      zeKernelSetArgumentValue(kernel, 2, sizeof(doWrite), &doWrite));

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &cmdList, 0));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListReset(cmdList));

  // Enqueue kernel to command list
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendLaunchKernel(
      cmdList, kernel, &dispatchTraits, event, 0, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));

  // Warmup
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &cmdList, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
  if (arguments.useEvents) {
    ASSERT_ZE_RESULT_SUCCESS(zeEventHostReset(event));
  }
  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    // Launch kernel
    timer.measureStart();
    ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
        levelzero.commandQueue, 1, &cmdList, 0));
    ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
        levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
    timer.measureEnd();

    if (arguments.useEvents) {
      ze_kernel_timestamp_result_t timestampResult{};
      ASSERT_ZE_RESULT_SUCCESS(
          zeEventQueryKernelTimestamp(event, &timestampResult));
      auto commandTime =
          std::chrono::nanoseconds(timestampResult.global.kernelEnd -
                                   timestampResult.global.kernelStart);
      commandTime *= timerResolution;
      statistics.pushValue(commandTime, bufferSize, typeSelector.getUnit(),
                           typeSelector.getType());
      ASSERT_ZE_RESULT_SUCCESS(zeEventHostReset(event));
    } else {
      statistics.pushValue(timer.get(), bufferSize, typeSelector.getUnit(),
                           typeSelector.getType());
    }
  }

  // Cleanup
  if (arguments.useEvents) {
    ASSERT_ZE_RESULT_SUCCESS(zeEventDestroy(event));
    ASSERT_ZE_RESULT_SUCCESS(zeEventPoolDestroy(eventPool));
  }
  ASSERT_ZE_RESULT_SUCCESS(zeMemFree(levelzero.context, buffer));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListDestroy(cmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeKernelDestroy(kernel));
  ASSERT_ZE_RESULT_SUCCESS(zeModuleDestroy(module));
  return TestResult::Success;
}

static RegisterTestCaseImplementation<SharedLocalMemoryEmbargo>
    registerTestCase(run, Api::L0);
