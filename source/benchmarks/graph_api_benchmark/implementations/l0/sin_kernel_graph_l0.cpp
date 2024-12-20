#include "framework/l0/levelzero.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/file_helper.h"
#include "framework/utility/timer.h"

#include "definitions/sin_kernel_graph.h"
#include "sin_common_l0.h"

#include <iostream>
#include <level_zero/ze_api.h>
#include <math.h>

// shape of model input (ABCD in general)
const size_t a = 2, b = 4, c = 8, d = 1024;
const size_t N = a * b * c * d;

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

  // Setup
  LevelZero levelzero;
  Timer timer;
  TestResult result = TestResult::Success;
  deviceMemMgr.init(&levelzero);

  // TODO: check if the device supports the required features

  // Create kernel
  auto spirvModuleA =
      FileHelper::loadBinaryFile("graph_api_benchmark_kernel_assign.spv");
  auto spirvModuleS =
      FileHelper::loadBinaryFile("graph_api_benchmark_kernel_sin.spv");

  if (spirvModuleA.size() == 0 || spirvModuleS.size() == 0) {
    return TestResult::KernelNotFound;
  }

  ze_module_handle_t moduleA, moduleS;
  ze_kernel_handle_t kernelA, kernelS;

  ze_module_desc_t moduleDescA{ZE_STRUCTURE_TYPE_MODULE_DESC};
  ze_module_desc_t moduleDescS{ZE_STRUCTURE_TYPE_MODULE_DESC};

  moduleDescA.format = moduleDescS.format = ZE_MODULE_FORMAT_IL_SPIRV;

  moduleDescA.pInputModule =
      reinterpret_cast<const uint8_t *>(spirvModuleA.data());
  moduleDescS.pInputModule =
      reinterpret_cast<const uint8_t *>(spirvModuleS.data());

  moduleDescA.inputSize = spirvModuleA.size();
  moduleDescS.inputSize = spirvModuleS.size();

  ze_kernel_desc_t kernelDescA{ZE_STRUCTURE_TYPE_KERNEL_DESC};
  ze_kernel_desc_t kernelDescS{ZE_STRUCTURE_TYPE_KERNEL_DESC};

  kernelDescA.pKernelName = "kernel_assign";
  kernelDescS.pKernelName = "kernel_sin";

  ASSERT_ZE_RESULT_SUCCESS(zeModuleCreate(levelzero.context, levelzero.device,
                                          &moduleDescA, &moduleA, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeModuleCreate(levelzero.context, levelzero.device,
                                          &moduleDescS, &moduleS, nullptr));

  ASSERT_ZE_RESULT_SUCCESS(zeKernelCreate(moduleA, &kernelDescA, &kernelA));
  ASSERT_ZE_RESULT_SUCCESS(zeKernelCreate(moduleS, &kernelDescS, &kernelS));

  // prepare host data for model input

  float *input_h;
  ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
  hostDesc.flags = 0;
  ASSERT_ZE_RESULT_SUCCESS(zeMemAllocHost(
      levelzero.context, &hostDesc, N * sizeof(float), 1, (void **)&input_h));

  for (size_t i = 0; i < N; ++i) {
    input_h[i] = random_float();
  }

  // warm up
  float *golden_h;
  ASSERT_ZE_RESULT_SUCCESS(zeMemAllocHost(
      levelzero.context, &hostDesc, N * sizeof(float), 1, (void **)&golden_h));

  Tensor4D input(a, b, c, d);

  // Host2Device for model input

  ze_command_list_handle_t cmdList;
  ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
  cmdListDesc.commandQueueGroupOrdinal = levelzero.commandQueueDesc.ordinal;
  cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListCreate(
      levelzero.context, levelzero.device, &cmdListDesc, &cmdList));

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
      cmdList, input.data, input_h, input.count() * sizeof(float), nullptr, 0,
      nullptr));

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &cmdList, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListReset(cmdList));

  Tensor4D output = run_model(input, numKernels, kernelA, kernelS, withGraphs,
                              levelzero.commandQueue, cmdList);

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
      cmdList, golden_h, output.data, output.count() * sizeof(float), nullptr,
      0, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &cmdList, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListReset(cmdList));

  deviceMemMgr.free(output.data, "output 133");

  // prepare command group for model input

  Tensor4D gr_input(a, b, c, d);

  // record graph here?
  std::cout << "Recording graph" << std::endl;

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
      cmdList, gr_input.data, input_h, input.count() * sizeof(float), nullptr,
      0, nullptr));

  Tensor4D gr_output = run_model(gr_input, numKernels, kernelA, kernelS,
                                 withGraphs, levelzero.commandQueue, cmdList);

  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));

  deviceMemMgr.free(gr_output.data, "gr_output 149");
  std::cout << "End of recording graph" << std::endl;

  float *output_h;
  ASSERT_ZE_RESULT_SUCCESS(zeMemAllocHost(
      levelzero.context, &hostDesc, N * sizeof(float), 1, (void **)&output_h));

  std::cout << "Creating command list" << std::endl;
  ze_command_list_handle_t execCmdList;
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListCreate(
      levelzero.context, levelzero.device, &cmdListDesc, &execCmdList));

  // compare the results in output_h and golden_h
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
              exit(-1);
            }
          }
        }
      }
    }
    return ret;
  };
  // do the check

  /* classic approach, to be replaced with graph call */
  gr_input.data = deviceMemMgr.alloc(N);

  std::cout << "Creating immediate command list" << std::endl;
  ze_command_list_handle_t immediateCmdList;
  ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
  cmdQueueDesc.pNext = nullptr;
  cmdQueueDesc.flags = 0;
  cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
  cmdQueueDesc.ordinal = levelzero.commandQueueDesc.ordinal;
  cmdQueueDesc.index = 0;
  cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
  zeCommandListCreateImmediate(levelzero.context, levelzero.device,
                               &cmdQueueDesc, &immediateCmdList);

  std::cout << "Appending cmd list" << std::endl;
  zeCommandListImmediateAppendCommandListsExp(immediateCmdList, 1, &cmdList,
                                              nullptr, 0, nullptr);

  std::cout << "Closing cmd list" << std::endl;
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(immediateCmdList));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
      levelzero.commandQueue, 1, &immediateCmdList, nullptr));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
      levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
  ASSERT_ZE_RESULT_SUCCESS(zeCommandListReset(immediateCmdList));

  // ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
  //     cmdList, gr_input.data, input_h, input.count() * sizeof(float),
  //     nullptr, 0, nullptr));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
  //     levelzero.commandQueue, 1, &cmdList, nullptr));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
  //     levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));

  // std::cout << __LINE__ << ": run_model" << std::endl;
  // gr_output = run_model(gr_input, numKernels, kernelA, kernelS, withGraphs,
  //                       levelzero.commandQueue, cmdList);

  // ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
  //     cmdList, output_h, gr_output.data, input.count() * sizeof(float),
  //     nullptr, 0, nullptr));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(cmdList));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
  //     levelzero.commandQueue, 1, &cmdList, nullptr));
  // ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
  //     levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));

  // deviceMemMgr.free(gr_output.data, "gr_output 210");
  /* end of classic approach */

  if (!check_result()) {
    std::cout << "Check FAILED" << std::endl;
    result = TestResult::Error;
  } else {
    int repeat = 100;

    for (std::size_t itr = 0; itr < arguments.iterations; ++itr) {
      timer.measureStart();

      if (!withGraphs) {
        for (int i = 0; i < repeat; ++i) {
          gr_input.data = deviceMemMgr.alloc(N);
          ASSERT_ZE_RESULT_SUCCESS(zeCommandListAppendMemoryCopy(
              execCmdList, gr_input.data, input_h,
              gr_input.count() * sizeof(float), nullptr, 0, nullptr));
          ASSERT_ZE_RESULT_SUCCESS(zeCommandListClose(execCmdList));
          ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueExecuteCommandLists(
              levelzero.commandQueue, 1, &execCmdList, nullptr));
          gr_output =
              run_model(gr_input, numKernels, kernelA, kernelS, withGraphs,
                        levelzero.commandQueue, execCmdList);

          deviceMemMgr.free(gr_output.data, "gr_output 235");
        }
      } else {
        for (int i = 0; i < repeat; ++i) {
          zeCommandListImmediateAppendCommandListsExp(execCmdList, 0, nullptr,
                                                      nullptr, 0, nullptr);
        }
      }
      ASSERT_ZE_RESULT_SUCCESS(zeCommandQueueSynchronize(
          levelzero.commandQueue, std::numeric_limits<uint64_t>::max()));
      ASSERT_ZE_RESULT_SUCCESS(zeCommandListReset(execCmdList));

      timer.measureEnd();

      statistics.pushValue(timer.get(), typeSelector.getUnit(),
                           typeSelector.getType());
    }
  }
  zeMemFree(levelzero.context, input_h);
  zeMemFree(levelzero.context, golden_h);

  deviceMemMgr.deinit();
  return result;
}

[[maybe_unused]] static RegisterTestCaseImplementation<SinKernelGraph>
    registerTestCase(run, Api::L0);