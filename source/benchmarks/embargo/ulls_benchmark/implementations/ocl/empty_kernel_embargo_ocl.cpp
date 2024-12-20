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

#include "framework/ocl/opencl.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/timer.h"

#include "definitions/empty_kernel_embargo.h"

#include <gtest/gtest.h>

static TestResult run(const EmptyKernelEmbargoArguments &arguments,
                      Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::Microseconds,
                                 MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  // Setup
  Opencl opencl;
  Timer timer;
  cl_int retVal;
  const size_t gws = arguments.workgroupCount * arguments.workgroupSize;
  const size_t lws = arguments.workgroupSize;

  // Create kernel
  const char *source = "__kernel void empty() {}";
  const auto sourceLength = strlen(source);
  cl_program program = clCreateProgramWithSource(opencl.context, 1, &source,
                                                 &sourceLength, &retVal);
  ASSERT_CL_SUCCESS(retVal);
  ASSERT_CL_SUCCESS(
      clBuildProgram(program, 1, &opencl.device, nullptr, nullptr, nullptr));
  cl_kernel kernel = clCreateKernel(program, "empty", &retVal);
  ASSERT_CL_SUCCESS(retVal);

  // Warmup, kernel
  ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(opencl.commandQueue, kernel, 1,
                                           nullptr, &gws, &lws, 0, nullptr,
                                           nullptr));
  ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));
  ASSERT_CL_SUCCESS(retVal);

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    // Enqueue empty kernel and measure it
    timer.measureStart();
    ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(opencl.commandQueue, kernel, 1,
                                             nullptr, &gws, &lws, 0, nullptr,
                                             nullptr));
    ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));
    timer.measureEnd();
    ASSERT_CL_SUCCESS(retVal);
    statistics.pushValue(timer.get(), typeSelector.getUnit(),
                         typeSelector.getType());
  }

  // Cleanup
  ASSERT_CL_SUCCESS(clReleaseKernel(kernel));
  ASSERT_CL_SUCCESS(clReleaseProgram(program));
  return TestResult::Success;
}

static RegisterTestCaseImplementation<EmptyKernelEmbargo>
    registerTestCase(run, Api::OpenCL);
