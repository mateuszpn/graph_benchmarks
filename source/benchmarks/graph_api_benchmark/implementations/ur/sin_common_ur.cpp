/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/ur/error.h"
#include "framework/ur/ur.h"

#include "sin_common_ur.h"

static constexpr size_t global_offset = 0;

Tensor4D run_kernel(const Tensor4D &input, ur_kernel_handle_t &kernel,
                    ur_queue_handle_t &queue, ur_event_handle_t *pWaitEvent,
                    ur_event_handle_t *pEvent, bool withGraphs,
                    ur_exp_command_buffer_handle_t *cmdBuf) {
  char kernelName[20];
  urKernelGetInfo(kernel, UR_KERNEL_INFO_FUNCTION_NAME, 20, kernelName,
                  nullptr);

  // std::cout << "run_kernel " << kernelName << std::endl;
  Tensor4D output(input.A, input.B, input.C, input.D);

  void *source = input.data;
  void *dest = output.data;

  EXPECT_UR_RESULT_SUCCESS(urKernelSetArgPointer(kernel, 0, nullptr, dest));
  EXPECT_UR_RESULT_SUCCESS(urKernelSetArgPointer(kernel, 1, nullptr, source));
  EXPECT_UR_RESULT_SUCCESS(
      urKernelSetArgValue(kernel, 2, sizeof(size_t), nullptr, &N));

  if (!withGraphs) {

    EXPECT_UR_RESULT_SUCCESS(urEnqueueKernelLaunch(
        queue, kernel, n_dimensions, &global_offset, global_size, nullptr,
        (pWaitEvent == nullptr) ? 0 : 1, pWaitEvent, pEvent));

  } else {

    assert(cmdBuf != nullptr && "cmdBuf is nullptr");
    EXPECT_UR_RESULT_SUCCESS(urCommandBufferAppendKernelLaunchExp(
        *cmdBuf, kernel, n_dimensions, &global_offset, global_size, nullptr, 0,
        nullptr, 0, nullptr, (pWaitEvent == nullptr) ? 0 : 1, pWaitEvent,
        nullptr, pEvent, nullptr));
  }
  return output;
}

Tensor4D run_model(Tensor4D &input, ur_queue_handle_t &queue,
                   int kernelIterations, bool withGraphs,
                   ur_event_handle_t *pEvent,
                   ur_exp_command_buffer_handle_t *cmdBuf) {

  ur_event_handle_t event;
  Tensor4D output =
      run_kernel(input, *pkA, queue, pEvent, &event, withGraphs, cmdBuf);

  std::vector<ur_event_handle_t> events(kernelIterations);
  input = output;
  output =
      run_kernel(input, *pkS, queue, &event, &events[0], withGraphs, cmdBuf);
  deviceMemMgr.free(input.data);

  for (int itr = 1; itr < kernelIterations; ++itr) {
    input = output;
    // output = run_kernel(input, *pkS, queue, &events[itr - 1], events[itr],
    //                     withGraphs, cmdBuf);
    output =
        run_kernel(input, *pkS, queue, nullptr, nullptr, withGraphs, cmdBuf);

    deviceMemMgr.free(input.data);
  }
  // if (!withGraphs) {
  // urEventWait(events.size(), events.data());
  // }

  return output;
}

DeviceMemoryManager deviceMemMgr;

void DeviceMemoryManager::init(UrState *ur, ur_usm_pool_handle_t *pool) {
  // assert(allocCnt == 0 && usedCnt == 0 && memInfos.size() == 0 &&
  //        "memory leak");
  this->ur = ur;
  this->pool = pool;
}

void DeviceMemoryManager::deinit() {
  // assert(usedCnt == 0 && "memory leak");

  for (; memInfos.size() > 0;) {
    DeviceMemoryInfo info = memInfos.back();
    // assert(!info.used && "memory leak");
    urUSMFree(ur->context, info.data);
    memInfos.pop_back();
    allocCnt--;
  }
  // assert(allocCnt == 0 && "memory leak");
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
  EXPECT_UR_RESULT_SUCCESS(urUSMDeviceAlloc(ur->context, ur->device, nullptr,
                                            *pool, count * sizeof(float),
                                            &deviceptr));
  DeviceMemoryInfo info;
  info.data = reinterpret_cast<float *>(deviceptr);
  info.count = count;
  info.used = true;
  memInfos.push_back(info);

  return info.data;
}

void DeviceMemoryManager::free(void *data) {
  for (auto &info : memInfos) {
    if (info.data == data) {
      // assert(info.used && "double free");
      info.used = false;
      usedCnt--;
      return;
    }
  }
  assert(!"should not reach here");
}