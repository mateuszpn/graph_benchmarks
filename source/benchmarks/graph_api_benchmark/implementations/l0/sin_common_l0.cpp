/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/l0/levelzero.h"

#include "sin_common_l0.h"
const ze_group_count_t groupCount{static_cast<uint32_t>(N), 1u, 1u};

Tensor4D run_kernel(const Tensor4D &input, ze_kernel_handle_t &kernel,
                    ze_command_list_handle_t &cmdList) {
  Tensor4D output(input.A, input.B, input.C, input.D);

  float *dest = output.data;
  float *source = input.data;

  zeKernelSetArgumentValue(kernel, 0, sizeof(float *), &dest);
  zeKernelSetArgumentValue(kernel, 1, sizeof(float *), &source);
  zeKernelSetArgumentValue(kernel, 2, sizeof(int), &N);

  zeCommandListAppendLaunchKernel(cmdList, kernel, &groupCount, nullptr, 0,
                                  nullptr);

  return output;
}

Tensor4D run_model(Tensor4D &input, int kernelIterations,
                   ze_kernel_handle_t &kernelA, ze_kernel_handle_t &kernelS,
                   bool withGraphs, ze_command_queue_handle_t &cmdQueue,
                   ze_command_list_handle_t &cmdList) {

  zeKernelSetGroupSize(kernelA, N, 1u, 1u);
  zeKernelSetGroupSize(kernelS, N, 1u, 1u);

  Tensor4D output = run_kernel(input, kernelA, cmdList);
  deviceMemMgr.free(input.data, "rm: input A");

  if (!withGraphs) {
    zeCommandListClose(cmdList);
    zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr);
    zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max());
    zeCommandListReset(cmdList);
  }

  for (int itr = 0; itr < kernelIterations; ++itr) {
    input = output;
    output = run_kernel(input, kernelS, cmdList);
    deviceMemMgr.free(input.data, "rm: input S");
  }

  if (!withGraphs) {
    zeCommandListClose(cmdList);
    zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr);
    zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max());
    zeCommandListReset(cmdList);
  }

  return output;
}

DeviceMemoryManager deviceMemMgr;

void DeviceMemoryManager::init(LevelZero *lz) {
  assert(allocCnt == 0 && usedCnt == 0 && memInfos.size() == 0 &&
         "memory leak");
  this->levelzero = lz;
}

void DeviceMemoryManager::deinit() {
  std::cout << "DeviceMemoryManager::deinit, usedCnt=" << usedCnt << std::endl;
  assert(usedCnt == 0 && "memory leak");

  for (; memInfos.size() > 0;) {
    DeviceMemoryInfo info = memInfos.back();
    assert(!info.used && "memory leak");
    zeMemFree(levelzero->context, info.data);
    memInfos.pop_back();
    allocCnt--;
  }
  assert(allocCnt == 0 && "memory leak");
}

float *DeviceMemoryManager::alloc(size_t count) {
  if (count == 0) {
    return nullptr;
  }

  for (auto &info : memInfos) {
    if (info.count >= count && !info.used) {
      usedCnt++;
      info.used = true;
      return info.data;
    }
  }

  usedCnt++;
  allocCnt++;
  void *deviceptr;
  ze_device_mem_alloc_desc_t deviceAllocationDesc = {
      ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

  zeMemAllocDevice(levelzero->context, &deviceAllocationDesc,
                   count * sizeof(float), 1, levelzero->device, &deviceptr);

  DeviceMemoryInfo info;
  info.data = reinterpret_cast<float *>(deviceptr);
  info.count = count;
  info.used = true;
  memInfos.push_back(info);
  return info.data;
}

void DeviceMemoryManager::free(void *data, const std::string name) {
  for (auto &info : memInfos) {
    if (info.data == data) {
      if (info.used == false)
        std::cout << "*** " << name << " ***" << std::endl;
      assert(info.used && "double free");
      info.used = false;
      usedCnt--;
      return;
    }
  }
  assert(!"should not reach here");
}