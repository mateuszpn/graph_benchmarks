#include "framework/test_case/register_test_case.h"
#include "framework/ur/error.h"
#include "framework/ur/ur.h"
#include "framework/utility/file_helper.h"
#include "framework/utility/timer.h"

#include "definitions/sin_kernel_graph.h"
#include "sin_common_ur.h"

#include <iostream>
#include <math.h>
#include <ur_api.h>

// shape of model input (ABCD in general)
const size_t a = 2, b = 4, c = 8, d = 1024;
const size_t N = a * b * c * d;

size_t n_dimensions = 1;
size_t global_size[] = {N};

ur_kernel_handle_t *pkA, *pkS;

static TestResult run(const SinKernelGraphArguments &arguments,
                      Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::Microseconds,
                                 MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  std::size_t numKernels = arguments.numKernels;
  bool withGraphs = arguments.withGraphs;

  // withGraphs ? std::cout << "Running WITH graphs\n"
  //            : std::cout << "Running WITHOUT graphs\n";

  // Setup
  UrState ur;
  Timer timer;
  TestResult result = TestResult::Success;

  ur_queue_handle_t queue;
  ur_queue_properties_t queueProperties{};

  EXPECT_UR_RESULT_SUCCESS(
      urQueueCreate(ur.context, ur.device, &queueProperties, &queue));
  ur_usm_pool_handle_t pool;
  ur_usm_pool_desc_t poolDesc = {};
  EXPECT_UR_RESULT_SUCCESS(urUSMPoolCreate(ur.context, &poolDesc, &pool));

  deviceMemMgr.init(&ur, &pool);

  // TODO: check if the device supports the required features

  // Create kernels
  auto spirvModuleA =
      FileHelper::loadBinaryFile("graph_api_benchmark_kernel_assign.spv");
  auto spirvModuleS =
      FileHelper::loadBinaryFile("graph_api_benchmark_kernel_sin.spv");

  if (spirvModuleA.size() == 0 || spirvModuleS.size() == 0)
    return TestResult::KernelNotFound;

  ur_kernel_handle_t kernelA, kernelS;

  ur_program_handle_t programA, programS;

  EXPECT_UR_RESULT_SUCCESS(
      urProgramCreateWithIL(ur.context, spirvModuleA.data(),
                            spirvModuleA.size(), nullptr, &programA));
  EXPECT_UR_RESULT_SUCCESS(urProgramBuild(ur.context, programA, nullptr));
  EXPECT_UR_RESULT_SUCCESS(urKernelCreate(programA, "kernel_assign", &kernelA));

  EXPECT_UR_RESULT_SUCCESS(
      urProgramCreateWithIL(ur.context, spirvModuleS.data(),
                            spirvModuleS.size(), nullptr, &programS));
  EXPECT_UR_RESULT_SUCCESS(urProgramBuild(ur.context, programS, nullptr));
  EXPECT_UR_RESULT_SUCCESS(urKernelCreate(programS, "kernel_sin", &kernelS));

  pkA = &kernelA;
  pkS = &kernelS;

  /* char kernelName[20];
  char kernelAttr[100];
  uint32_t numArgs, refCnt;

  urKernelGetInfo(kernelA, UR_KERNEL_INFO_FUNCTION_NAME, 20, kernelName,
                  nullptr);
  urKernelGetInfo(kernelA, UR_KERNEL_INFO_NUM_ARGS, 8, &numArgs, nullptr);
  urKernelGetInfo(kernelA, UR_KERNEL_INFO_REFERENCE_COUNT, 8, &refCnt, nullptr);
  urKernelGetInfo(kernelA, UR_KERNEL_INFO_ATTRIBUTES, 100, kernelAttr, nullptr);

  std::cout << "Kernel name: " << kernelName << " numArgs:" << numArgs
            << " attrs:" << kernelAttr << std::endl;

  urKernelGetInfo(kernelS, UR_KERNEL_INFO_FUNCTION_NAME, 20, kernelName,
                  nullptr);
  urKernelGetInfo(kernelS, UR_KERNEL_INFO_NUM_ARGS, 8, &numArgs, nullptr);
  urKernelGetInfo(kernelS, UR_KERNEL_INFO_REFERENCE_COUNT, 8, &refCnt, nullptr);
  urKernelGetInfo(kernelS, UR_KERNEL_INFO_ATTRIBUTES, 100, kernelAttr, nullptr);

  std::cout << "Kernel name: " << kernelName << " numArgs:" << numArgs
            << " attrs:" << kernelAttr << std::endl; */

  // prepare host data for model input

  std::vector<float> input_h(N);

  for (size_t i = 0; i < N; ++i) {
    input_h[i] = random_float();
  }

  // warm up
  std::vector<float> golden_h(N);

  ur_event_handle_t event1, event2;
  Tensor4D input(a, b, c, d);

  EXPECT_UR_RESULT_SUCCESS(
      urEnqueueUSMMemcpy(queue, false, input.data, input_h.data(),
                         input.count() * sizeof(float), 0, nullptr, &event1));
  EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &event1));

  // std::cout << "input_h[0] = " << input_h[0] << std::endl;

  Tensor4D output =
      run_model(input, queue, numKernels, false, nullptr, nullptr);

  std::cout << "run_model done" << std::endl;

  EXPECT_UR_RESULT_SUCCESS(
      urEnqueueUSMMemcpy(queue, false, golden_h.data(), output.data,
                         output.count() * sizeof(float), 0, nullptr, &event2));
  EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &event2));

  std::cout << "golden_h[0] = " << golden_h[0] << std::endl;
  std::cout << "golden_h[1024] = " << golden_h[1024] << std::endl;

  deviceMemMgr.free(input.data);
  deviceMemMgr.free(output.data);

  Tensor4D gr_input(a, b, c, d);

  // here to start command buffer
  // if (!withGraphs), cmdBuffer is ignored

  std::cout << "create command buffer" << std::endl;
  ur_exp_command_buffer_handle_t cmdBuffer;
  ur_exp_command_buffer_desc_t cmdBufferDesc = {};
  cmdBufferDesc.isInOrder;
  urCommandBufferCreateExp(ur.context, ur.device, &cmdBufferDesc, &cmdBuffer);
  Tensor4D gr_output =
      run_model(gr_input, queue, numKernels, withGraphs, nullptr, &cmdBuffer);
  urCommandBufferFinalizeExp(cmdBuffer);

  std::cout << "create command buffer done" << std::endl;

  std::vector<float> output_h(gr_output.count());

  ur_queue_handle_t exec_q;
  //, copy_q, bench_q;
  ur_event_handle_t event_q;

  EXPECT_UR_RESULT_SUCCESS(
      urQueueCreate(ur.context, ur.device, &queueProperties, &exec_q));

  EXPECT_UR_RESULT_SUCCESS(urEnqueueUSMMemcpy(
      exec_q, true, gr_input.data, input_h.data(),
      gr_input.count() * sizeof(float), 0, nullptr, &event_q));
  // EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &event_q));

  auto check_result = [&] {
    bool ret = true;
    for (int ai = 0; ai < gr_output.A; ++ai) {
      for (int bi = 0; bi < gr_output.B; ++bi) {
        for (int ci = 0; ci < gr_output.C; ++ci) {
          for (int di = 0; di < gr_output.D; ++di) {
            int index = di + ci * gr_output.D + bi * gr_output.C * gr_output.D +
                        ai * gr_output.B * gr_output.C * gr_output.D;
            if ((fabs(output_h[index] - golden_h[index]) > 0.0001f) ||
                (fabs(output_h[index]) == 0.0f)) {
              ret = false;
              std::cout << "at (" << ai << ", " << bi << ", " << ci << ", "
                        << di << "), expect " << golden_h[index] << ", but got "
                        << output_h[index] << std::endl;
              exit(1);
            }
          }
        }
      }
    }
    return ret;
  };

  // run the model directly or the graph, and do the result check

  if (!withGraphs) {
    std::cout << "run the model (no graphs)" << std::endl;
    ur_event_handle_t ngEvent;
    gr_output =
        run_model(gr_input, exec_q, numKernels, false, &event_q, nullptr);
    EXPECT_UR_RESULT_SUCCESS(urEnqueueUSMMemcpy(
        exec_q, false, output_h.data(), gr_output.data,
        gr_output.count() * sizeof(float), 0, nullptr, &ngEvent));
    EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &ngEvent));
    EXPECT_UR_RESULT_SUCCESS(urQueueFinish(exec_q));

  } else {
    ur_event_handle_t bufEvent, grEvent;
    // SEE
    // oneapi-src.github.io/unified-runtime/core/EXP-COMMAND-BUFFER.html

    std::cout << "urCommandBufferEnqueueExp - run the model (with graphs)"
              << std::endl;
    EXPECT_UR_RESULT_SUCCESS(
        urCommandBufferEnqueueExp(cmdBuffer, exec_q, 1, &event_q, &bufEvent));
    std::cout << "event wait" << std::endl;

    EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &bufEvent));

    std::cout << "urEnqueueUSMMemcpy" << std::endl;
    EXPECT_UR_RESULT_SUCCESS(urEnqueueUSMMemcpy(
        exec_q, false, output_h.data(), gr_output.data,
        gr_output.count() * sizeof(float), 0, nullptr, &grEvent));
    std::cout << "urQueueFinish " << std::endl;
    EXPECT_UR_RESULT_SUCCESS(urQueueFinish(exec_q));
  }

  std::cout << "output_h[0] = " << output_h[0] << std::endl;
  std::cout << "output_h[1024] = " << output_h[1024] << std::endl;

  if (!check_result()) {
    std::cout << "Check FAILED" << std::endl;
    result = TestResult::Error;
  } else {
    int repeat = 100;
    for (std::size_t i = 0; i < arguments.iterations; ++i) {
      Tensor4D bm_input(a, b, c, d);
      Tensor4D bm_output(1, 1, 1, 1);

      if (!withGraphs) {
        ur_event_handle_t bmEvent;
        timer.measureStart();

        for (int i = 0; i < repeat; ++i) {
          deviceMemMgr.free(bm_output.data);
          EXPECT_UR_RESULT_SUCCESS(urEnqueueUSMMemcpy(
              exec_q, true, bm_input.data, input_h.data(),
              bm_input.count() * sizeof(float), 0, nullptr, &bmEvent));
          EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &bmEvent));

          bm_output =
              run_model(bm_input, exec_q, numKernels, false, nullptr, nullptr);
        }
        timer.measureEnd();

        deviceMemMgr.free(bm_input.data);
        deviceMemMgr.free(bm_output.data);
      } else {
        // run with graph
        timer.measureStart();
        for (int i = 0; i < repeat; ++i) {
          ur_event_handle_t cpEvent, grEvent;
          EXPECT_UR_RESULT_SUCCESS(urEnqueueUSMMemcpy(
              exec_q, true, bm_input.data, input_h.data(),
              bm_input.count() * sizeof(float), 0, nullptr, &cpEvent));
          EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &cpEvent));

          EXPECT_UR_RESULT_SUCCESS(urCommandBufferEnqueueExp(
              cmdBuffer, exec_q, 0, nullptr, &grEvent));
          EXPECT_UR_RESULT_SUCCESS(urEventWait(1, &grEvent));
        }
        timer.measureEnd();
      }
      statistics.pushValue(timer.get(), typeSelector.getUnit(),
                           typeSelector.getType());
    }
  }
  deviceMemMgr.free(gr_input.data);
  deviceMemMgr.free(gr_output.data);

  deviceMemMgr.deinit();
  return result;
}

[[maybe_unused]] static RegisterTestCaseImplementation<SinKernelGraph>
    registerTestCase(run, Api::UR);