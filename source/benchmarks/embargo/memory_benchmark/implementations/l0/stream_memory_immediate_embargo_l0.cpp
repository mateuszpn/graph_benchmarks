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

#include "definitions/stream_memory_immediate_embargo.h"

#include <gtest/gtest.h>

using namespace MemoryConstants;

static TestResult run(const StreamMemoryImmediateEmbargoArguments &arguments,
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
  Timer timer;

  // Query double support
  ze_device_module_properties_t moduleProperties{};
  ASSERT_ZE_RESULT_SUCCESS(
      zeDeviceGetModuleProperties(levelzero.device, &moduleProperties));

  const bool useDoubles = moduleProperties.fp64flags != 0u;
  const size_t elementSize = useDoubles ? sizeof(double) : sizeof(float);
  const size_t fillValue = 313u;
  const int32_t scalarValue = -999;
  const uint32_t gws = static_cast<uint32_t>(arguments.size / elementSize);
  const uint64_t timerResolution =
      levelzero.getTimerResolution(levelzero.device);

  // Create module
  const char *kernelFile = useDoubles
                               ? "memory_benchmark_stream_memory_fp64.spv"
                               : "memory_benchmark_stream_memory.spv";
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

  // Create buffers
  size_t bufferSize = arguments.size;
  void *buffers[3] = {};
  size_t buffersCount = {};
  size_t bufferSizes[3] = {bufferSize, bufferSize, bufferSize};

  const ze_device_mem_alloc_desc_t deviceAllocationDesc{
      ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
  ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
  const size_t reduction = 4;
  switch (arguments.type) {
  case StreamMemoryEmbargoType::Stream_3BytesRGBtoY:
  case StreamMemoryEmbargoType::Stream_3BytesAlignedRGBtoY:
    ASSERT_ZE_RESULT_SUCCESS(
        zeMemAllocDevice(levelzero.context, &deviceAllocationDesc, bufferSize,
                         0, levelzero.device, &buffers[buffersCount++]));
    bufferSizes[buffersCount] = bufferSize / reduction;
    ASSERT_ZE_RESULT_SUCCESS(zeMemAllocDevice(
        levelzero.context, &deviceAllocationDesc, bufferSize / reduction, 0,
        levelzero.device, &buffers[buffersCount++]));
    if (arguments.type == StreamMemoryEmbargoType::Stream_3BytesRGBtoY) {
      kernelDesc.pKernelName = "stream_3bytesRGBtoY";
    } else {
      kernelDesc.pKernelName = "stream_3BytesAlignedRGBtoY";
    }
    break;
  default:
    FATAL_ERROR("Unknown StreamMemoryType");
  }

  // Create kernel
  ASSERT_ZE_RESULT_SUCCESS(zeKernelCreate(module, &kernelDesc, &kernel));

  // Query maximum group size
  const uint32_t groupSizeX =
      levelzero.getDeviceComputeProperties().maxGroupSizeX;

  // Configure kernel group size
  ASSERT_ZE_RESULT_SUCCESS(zeKernelSetGroupSize(kernel, groupSizeX, 1u, 1u));
  const ze_group_count_t dispatchTraits{gws / groupSizeX, 1u, 1u};

  ze_command_list_handle_t cmdList;
  ze_command_queue_desc_t commandQueueDesc = levelzero.commandQueueDesc;
  commandQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListCreateImmediate(
      levelzero.context, levelzero.device, &commandQueueDesc, &cmdList));

  // Create event
  ze_event_pool_flags_t eventPoolFlags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
  if (arguments.useEvents) {
    eventPoolFlags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
  }
  ze_event_pool_handle_t eventPool{};
  ze_event_handle_t event{};
  ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
  eventPoolDesc.flags = eventPoolFlags;
  eventPoolDesc.count = 1;
  ASSERT_ZE_RESULT_SUCCESS(zeEventPoolCreate(levelzero.context, &eventPoolDesc,
                                             1, &levelzero.commandQueueDevice,
                                             &eventPool));
  ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC};
  eventDesc.index = 0;
  eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
  eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
  ASSERT_ZE_RESULT_SUCCESS(zeEventCreate(eventPool, &eventDesc, &event));

  // Enqueue filling of the buffers and set kernel arguments
  for (auto i = 0u; i < buffersCount; i++) {
    ze_event_handle_t eventForMemoryFill = event;
    ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryFill(
        cmdList, buffers[i], &fillValue, sizeof(fillValue), bufferSizes[i],
        eventForMemoryFill, 0, nullptr));
    ASSERT_ZE_RESULT_SUCCESS(zeKernelSetArgumentValue(
        kernel, static_cast<int>(i), sizeof(buffers[i]), &buffers[i]));
    ASSERT_ZE_RESULT_SUCCESS(
        zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    ASSERT_ZE_RESULT_SUCCESS(zeEventHostReset(event));
  }
  ASSERT_ZE_RESULT_SUCCESS(
      zeKernelSetArgumentValue(kernel, static_cast<uint32_t>(buffersCount),
                               sizeof(scalarValue), &scalarValue));

  // Warmup
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendLaunchKernel(
      cmdList, kernel, &dispatchTraits, event, 0, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(
      zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
  ASSERT_ZE_RESULT_SUCCESS(zeEventHostReset(event));

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    // Launch kernel
    timer.measureStart();
    ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendLaunchKernel(
        cmdList, kernel, &dispatchTraits, event, 0, nullptr));
    ASSERT_ZE_RESULT_SUCCESS(
        zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    timer.measureEnd();

    size_t transferSize = arguments.size;
    switch (arguments.type) {
    case StreamMemoryEmbargoType::Stream_3BytesRGBtoY:
    case StreamMemoryEmbargoType::Stream_3BytesAlignedRGBtoY:
      transferSize = (transferSize / reduction) +
                     (3 * transferSize / 4); // 3B Read + 1B Write
      break;
    default:
      break;
    }

    if (arguments.useEvents) {
      ze_kernel_timestamp_result_t timestampResult{};
      ASSERT_ZE_RESULT_SUCCESS(
          zeEventQueryKernelTimestamp(event, &timestampResult));
      auto commandTime =
          std::chrono::nanoseconds(timestampResult.global.kernelEnd -
                                   timestampResult.global.kernelStart);
      commandTime *= timerResolution;
      statistics.pushValue(commandTime, transferSize, typeSelector.getUnit(),
                           typeSelector.getType());
    } else {
      statistics.pushValue(timer.get(), transferSize, typeSelector.getUnit(),
                           typeSelector.getType());
    }
    ASSERT_ZE_RESULT_SUCCESS(zeEventHostReset(event));
  }

  // Cleanup
  ASSERT_ZE_RESULT_SUCCESS(zeEventDestroy(event));
  ASSERT_ZE_RESULT_SUCCESS(zeEventPoolDestroy(eventPool));

  for (size_t i = 0; i < buffersCount; i++) {
    ASSERT_ZE_RESULT_SUCCESS(zeMemFree(levelzero.context, buffers[i]));
  }
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListDestroy(cmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeKernelDestroy(kernel));
  ASSERT_ZE_RESULT_SUCCESS(zeModuleDestroy(module));
  return TestResult::Success;
}

static RegisterTestCaseImplementation<StreamMemoryImmediateEmbargo>
    registerTestCase(run, Api::L0);
