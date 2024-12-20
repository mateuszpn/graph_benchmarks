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

#include "framework/ocl/opencl.h"
#include "framework/ocl/utility/buffer_contents_helper_ocl.h"
#include "framework/ocl/utility/profiling_helper.h"
#include "framework/ocl/utility/program_helper_ocl.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/compiler_options_builder.h"
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
  cl_int retVal = {};
  QueueProperties queueProperties =
      QueueProperties::create().setProfiling(true).setOoq(0);
  Opencl opencl(queueProperties);
  Timer timer;

  size_t maxSlmSize = {};
  clGetDeviceInfo(opencl.device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(maxSlmSize),
                  &maxSlmSize, nullptr);
  if (arguments.slmSize > maxSlmSize) {
    return TestResult::DeviceNotCapable;
  }

  const size_t fillValue = 0;
  const bool printBuildInfo = true;

  // Create kernel-specific buffer
  size_t bufferSize = 1024 * kiloByte;
  cl_mem buffer = clCreateBuffer(opencl.context, CL_MEM_READ_WRITE, bufferSize,
                                 nullptr, &retVal);
  // Create kernel
  cl_program program{};
  if (auto result = ProgramHelperOcl::buildProgramFromSourceFile(
          opencl.context, opencl.device, "shared_local_memory.cl", nullptr,
          program);
      result != TestResult::Success) {
    if (result != TestResult::Success && printBuildInfo) {
      size_t numBytes = 0;
      retVal |= clGetProgramBuildInfo(program, opencl.device,
                                      CL_PROGRAM_BUILD_LOG, 0, NULL, &numBytes);
      auto bytes = std::make_unique<char[]>(numBytes);
      retVal |=
          clGetProgramBuildInfo(program, opencl.device, CL_PROGRAM_BUILD_LOG,
                                numBytes, bytes.get(), &numBytes);
      std::cout << bytes.get() << std::endl;
    }
    return result;
  }

  const cl_int doWrite = 0;
  cl_kernel kernel = clCreateKernel(program, "slm_alloc_size", &retVal);
  ASSERT_CL_SUCCESS(retVal);
  ASSERT_CL_SUCCESS(clEnqueueFillBuffer(opencl.commandQueue, buffer, &fillValue,
                                        sizeof(fillValue), 0, bufferSize, 0,
                                        nullptr, nullptr));
  ASSERT_CL_SUCCESS(clSetKernelArg(kernel, 0, sizeof(buffer), &buffer));
  ASSERT_CL_SUCCESS(clSetKernelArg(kernel, 1, arguments.slmSize, nullptr));
  ASSERT_CL_SUCCESS(clSetKernelArg(kernel, 2, sizeof(doWrite), &doWrite));

  // Warm up
  const size_t globalWorkSize = bufferSize / 4;
  const size_t localWorkSize = 256;
  ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(
      opencl.commandQueue, kernel, 1, nullptr, &globalWorkSize, &localWorkSize,
      0, nullptr, nullptr));
  ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));

  for (auto i = 0u; i < arguments.iterations; i++) {
    cl_event profilingEvent{};
    cl_event *eventForEnqueue = arguments.useEvents ? &profilingEvent : nullptr;

    timer.measureStart();
    ASSERT_CL_SUCCESS(clEnqueueNDRangeKernel(
        opencl.commandQueue, kernel, 1, nullptr, &globalWorkSize,
        &localWorkSize, 0, nullptr, eventForEnqueue));
    ASSERT_CL_SUCCESS(clFinish(opencl.commandQueue));
    timer.measureEnd();

    if (eventForEnqueue) {
      cl_ulong timeNs{};
      ASSERT_CL_SUCCESS(ProfilingHelper::getEventDurationInNanoseconds(
          profilingEvent, timeNs));
      ASSERT_CL_SUCCESS(clReleaseEvent(profilingEvent));

      statistics.pushValue(std::chrono::nanoseconds(timeNs), bufferSize,
                           typeSelector.getUnit(), typeSelector.getType());
    } else {
      statistics.pushValue(timer.get(), bufferSize, typeSelector.getUnit(),
                           typeSelector.getType());
    }
  }

  // Cleanup
  ASSERT_CL_SUCCESS(clReleaseMemObject(buffer));
  ASSERT_CL_SUCCESS(clReleaseKernel(kernel));
  ASSERT_CL_SUCCESS(clReleaseProgram(program));
  return TestResult::Success;
}

static RegisterTestCaseImplementation<SharedLocalMemoryEmbargo>
    registerTestCase(run, Api::OpenCL);
