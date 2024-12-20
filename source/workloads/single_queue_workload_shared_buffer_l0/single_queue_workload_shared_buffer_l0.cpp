/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/l0/levelzero.h"
#include "framework/utility/file_helper.h"
#include "framework/utility/timer.h"
#include "framework/workload/register_workload.h"

#include <cstring>

struct SingleQueueWorkloadSharedBufferArguments : WorkloadArgumentContainer {
  PositiveIntegerArgument operationsCount;
  PositiveIntegerArgument workgroupCount;
  PositiveIntegerArgument workgroupSize;
  IntegerArgument bufferIpcHandle;
  NonNegativeIntegerArgument offsetWithinBuffer;

  SingleQueueWorkloadSharedBufferArguments()
      : operationsCount(*this, "operationsCount",
                        "Number of redundant operations performed in kernel to "
                        "make it take longer"),
        workgroupCount(*this, "wgc", "Number of workgroups enqueued"),
        workgroupSize(*this, "wgs", "Size of workgroups enqueued"),
        bufferIpcHandle(
            *this, "bufferIpcHandle",
            "Handle of the LevelZero buffer acquired from zeMemGetIpcHandle"),
        offsetWithinBuffer(*this, "bufferOffset",
                           "Offset within the buffer described by IPC handle") {
  }
};

struct SingleQueueWorkloadSharedBuffer
    : Workload<SingleQueueWorkloadSharedBufferArguments> {};

TestResult run(const SingleQueueWorkloadSharedBufferArguments &arguments,
               Statistics &statistics, WorkloadSynchronization &synchronization,
               WorkloadIo &io) {
  LevelZero levelzero{};
  Timer timer{};

  // Ensure we're running on a single tile
  uint32_t tilesCount = {};
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(
      zeDeviceGetSubDevices(levelzero.device, &tilesCount, nullptr));
  if (tilesCount > 1) {
    io.writeToConsole("This workload should run on a single tile\n");
    return TestResult::DeviceNotCapable;
  }

  // Compute work size
  const auto totalThreadsCount =
      arguments.workgroupSize * arguments.workgroupCount;
  const ze_group_count_t groupCount{
      static_cast<uint32_t>(arguments.workgroupCount), 1, 1};
  const auto operationsCount = static_cast<uint32_t>(arguments.operationsCount);
  const auto bufferSizeInBytes = totalThreadsCount * sizeof(uint32_t);

  // Prepare IPC Handle
  ze_ipc_mem_handle_t ipcHandle = {};
  std::memcpy(ipcHandle.data, arguments.bufferIpcHandle.getAddressOf(),
              arguments.bufferIpcHandle.getSizeOf());

  // Create buffer
  void *bufferBase = nullptr;
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeMemOpenIpcHandle(
      levelzero.context, levelzero.device, ipcHandle, 0, &bufferBase));
  void *buffer =
      static_cast<uint8_t *>(bufferBase) + arguments.offsetWithinBuffer;
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeContextMakeMemoryResident(
      levelzero.context, levelzero.device, bufferBase, bufferSizeInBytes));

  // Create kernel
  auto spirvModule =
      FileHelper::loadBinaryFile("single_queue_workload_increment.spv");
  if (spirvModule.size() == 0) {
    return TestResult::KernelNotFound;
  }
  ze_module_handle_t module{};
  ze_kernel_handle_t kernel{};
  ze_module_desc_t moduleDesc{ZE_STRUCTURE_TYPE_MODULE_DESC};
  moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
  moduleDesc.pInputModule =
      reinterpret_cast<const uint8_t *>(spirvModule.data());
  moduleDesc.inputSize = spirvModule.size();
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeModuleCreate(
      levelzero.context, levelzero.device, &moduleDesc, &module, nullptr));
  ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
  kernelDesc.pKernelName = "increment";
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(
      zeKernelCreate(module, &kernelDesc, &kernel));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeKernelSetGroupSize(
      kernel, static_cast<uint32_t>(arguments.workgroupSize), 1u, 1u));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(
      zeKernelSetArgumentValue(kernel, 0, sizeof(buffer), &buffer));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeKernelSetArgumentValue(
      kernel, 1, sizeof(operationsCount), &operationsCount));

  // Create command list
  ze_command_list_desc_t cmdListDesc{};
  cmdListDesc.commandQueueGroupOrdinal = levelzero.commandQueueDesc.ordinal;
  ze_command_list_handle_t cmdList{};
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandListCreate(
      levelzero.context, levelzero.device, &cmdListDesc, &cmdList));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandListAppendLaunchKernel(
      cmdList, kernel, &groupCount, nullptr, 0, nullptr));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandListClose(cmdList));

  // Warmup
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &cmdList, nullptr));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    synchronization.synchronize(io);

    timer.measureStart();
    ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
        levelzero.commandQueue, 1, &cmdList, nullptr));
    ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
        levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
    timer.measureEnd();

    statistics.pushValue(timer.get(), MeasurementUnit::Unknown,
                         MeasurementType::Unknown);
  }

  // Evict buffer
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeContextEvictMemory(
      levelzero.context, levelzero.device, buffer, bufferSizeInBytes));

  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeCommandListDestroy(cmdList));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeKernelDestroy(kernel));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(zeModuleDestroy(module));
  ZE_RESULT_SUCCESS_OR_RETURN_ERROR(
      zeMemCloseIpcHandle(levelzero.context, bufferBase));
  return TestResult::Success;
}

int main(int argc, char **argv) {
  SingleQueueWorkloadSharedBuffer workload;
  SingleQueueWorkloadSharedBuffer::implementation = run;
  return workload.runFromCommandLine(argc, argv);
}
