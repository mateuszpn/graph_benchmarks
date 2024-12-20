/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "framework/ocl/opencl.h"
#include "framework/ocl/utility/compression_helper.h"
#include "framework/ocl/utility/profiling_helper.h"
#include "framework/ocl/utility/usm_helper_ocl.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/timer.h"

#include "definitions/usm_shared_migrate_gpu.h"

#include <gtest/gtest.h>

static TestResult run(const UsmSharedMigrateGpuArguments &arguments,
                      Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::GigabytesPerSecond,
                                 MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  // Setup
  const DeviceSelection queuePlacement =
      DeviceSelectionHelper::withoutHost(arguments.bufferPlacement);
  QueueProperties queueProperties = QueueProperties::create()
                                        .setDeviceSelection(queuePlacement)
                                        .allowCreationFail();
  ContextProperties contextProperties =
      ContextProperties::create()
          .setDeviceSelection(arguments.contextPlacement)
          .allowCreationFail();
  Opencl opencl(queueProperties, contextProperties);
  if (opencl.context == nullptr || opencl.commandQueue == nullptr) {
    return TestResult::DeviceNotCapable;
  }
  auto clMemFreeINTEL =
      (pfn_clMemFreeINTEL)clGetExtensionFunctionAddressForPlatform(
          opencl.platform, "clMemFreeINTEL");
  if (!opencl.getExtensions().isUsmSupported()) {
    return TestResult::DriverFunctionNotFound;
  }

  Timer timer;
  cl_int retVal;

  // Create buffer
  cl_int *buffer = static_cast<cl_int *>(UsmHelperOcl::allocate(
      arguments.bufferPlacement, opencl, arguments.bufferSize, &retVal));
  ASSERT_CL_SUCCESS(retVal);
  const size_t elementsCount = arguments.bufferSize / sizeof(cl_int);

  // Create kernel
  const char *source = "kernel void fill_with_ones(__global int *buffer) { "
                       "const uint gid = get_global_id(0);  buffer[gid] = 1; }";
  const auto sourceLength = strlen(source);
  cl_program program = clCreateProgramWithSource(opencl.context, 1, &source,
                                                 &sourceLength, &retVal);
  ASSERT_CL_SUCCESS(retVal);
  ASSERT_CL_SUCCESS(
      clBuildProgram(program, 0u, nullptr, nullptr, nullptr, nullptr));
  cl_kernel kernel = clCreateKernel(program, "fill_with_ones", &retVal);
  ASSERT_CL_SUCCESS(retVal);

  // Warmup
  for (auto elementIndex = 0u; elementIndex < elementsCount; elementIndex++) {
    buffer[elementIndex] = 0;
  }
  const auto gws = elementsCount;
  ASSERT_CL_SUCCESS(clSetKernelArgSVMPointer(kernel, 0, buffer));
  ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(opencl.commandQueue, kernel, 1,
                                           nullptr, &gws, nullptr, 0, nullptr,
                                           nullptr));
  ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    // Migrate whole resource to CPU
    for (auto elementIndex = 0u; elementIndex < elementsCount; elementIndex++) {
      buffer[elementIndex] = 0;
    }

    // Measure kernel which must migrate the resource to GPU
    timer.measureStart();
    ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(opencl.commandQueue, kernel, 1,
                                             nullptr, &gws, nullptr, 0, nullptr,
                                             nullptr));
    ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));
    timer.measureEnd();

    statistics.pushValue(timer.get(), arguments.bufferSize,
                         typeSelector.getUnit(), typeSelector.getType());
  }

  ASSERT_CL_SUCCESS(clReleaseKernel(kernel));
  ASSERT_CL_SUCCESS(clReleaseProgram(program));
  ASSERT_CL_SUCCESS(clMemFreeINTEL(opencl.context, buffer));
  return TestResult::Success;
}

static RegisterTestCaseImplementation<UsmSharedMigrateGpu>
    registerTestCase(run, Api::OpenCL, true);
