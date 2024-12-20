/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2022-2023 Intel Corporation
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

#include "framework/sycl/sycl.h"
#include "framework/test_case/register_test_case.h"
#include "framework/utility/memory_constants.h"
#include "framework/utility/timer.h"

#include "definitions/stream_memory_embargo.h"

#include <gtest/gtest.h>

using namespace MemoryConstants;

class StreamMemoryEmbargoBenchmark {
public:
  virtual ~StreamMemoryEmbargoBenchmark() = default;
  virtual sycl::event run(sycl::queue &) = 0;
  virtual size_t transferSize() const = 0;
};

// Stream 3bytesRGBtoY benchmark

class Stream3BytesRGBToYBenchmark : public StreamMemoryEmbargoBenchmark {
public:
  Stream3BytesRGBToYBenchmark(size_t bufferSize, uint8_t fillValue)
      : bufferX(bufferSize, fillValue),
        bufferLuminance(bufferSize / 4u, fillValue),
        deviceBufferX(this->bufferX.data(),
                      sycl::range<1>{this->bufferX.size()}),
        deviceBufferLuminance(this->bufferLuminance.data(),
                              sycl::range<1>{this->bufferLuminance.size()}) {}

  sycl::event run(sycl::queue &queue) override {
    auto commandList = [&](sycl::handler &cgh) {
      const size_t luminanceSize = this->bufferLuminance.size();
      auto x =
          this->deviceBufferX.template get_access<sycl::access_mode::read>(cgh);
      auto luminance =
          this->deviceBufferLuminance
              .template get_access<sycl::access_mode::discard_write>(cgh);

      cgh.parallel_for<class Stream3BytesRGBToYKernel>(
          sycl::range<1>{luminanceSize}, [=](sycl::item<1> item) {
            const int i = item.get_id() * 3;
            luminance[item] = static_cast<uint8_t>(
                x[i] * 0.2126f + x[i + 1] * 0.7152f + x[i + 2] * 0.0722f);
          });
    };
    return queue.submit(commandList);
  }

  size_t transferSize() const override {
    return this->bufferX.size() + this->bufferLuminance.size();
  }

protected:
  std::vector<uint8_t> bufferX;
  std::vector<uint8_t> bufferLuminance;
  sycl::buffer<uint8_t, 1> deviceBufferX;
  sycl::buffer<uint8_t, 1> deviceBufferLuminance;
};

// Stream 3bytesAlignedRGBtoY benchmark

class Stream3BytesAlignedRGBToYBenchmark : public StreamMemoryEmbargoBenchmark {
  using uchar4 = sycl::vec<uint8_t, 4>;

public:
  Stream3BytesAlignedRGBToYBenchmark(size_t bufferSize, uint8_t fillValue)
      : bufferX(bufferSize / uchar4::size(),
                uchar4{fillValue, fillValue, fillValue, 0u}),
        bufferLuminance(bufferSize / 4u, fillValue),
        deviceBufferX(this->bufferX.data(),
                      sycl::range<1>{this->bufferX.size()}),
        deviceBufferLuminance(this->bufferLuminance.data(),
                              sycl::range<1>{this->bufferLuminance.size()}) {}

  sycl::event run(sycl::queue &queue) override {
    auto commandList = [&](sycl::handler &cgh) {
      const size_t luminanceSize = this->bufferLuminance.size();
      auto x =
          this->deviceBufferX.template get_access<sycl::access_mode::read>(cgh);
      auto luminance =
          this->deviceBufferLuminance
              .template get_access<sycl::access_mode::discard_write>(cgh);

      cgh.parallel_for<class Stream3BytesAlignedRGBToYKernel>(
          sycl::range<1>{luminanceSize}, [=](sycl::item<1> item) {
            luminance[item] = static_cast<uint8_t>(x[item].x() * 0.2126f +
                                                   x[item].y() * 0.7152f +
                                                   x[item].z() * 0.0722f);
          });
    };
    return queue.submit(commandList);
  }

  size_t transferSize() const override {
    return this->bufferX.size() + this->bufferLuminance.size();
  }

protected:
  std::vector<uchar4> bufferX;
  std::vector<uint8_t> bufferLuminance;
  sycl::buffer<uchar4, 1> deviceBufferX;
  sycl::buffer<uint8_t, 1> deviceBufferLuminance;
};

static TestResult run(const StreamMemoryEmbargoArguments &arguments,
                      Statistics &statistics) {
  MeasurementFields typeSelector(MeasurementUnit::GigabytesPerSecond,
                                 arguments.useEvents ? MeasurementType::Gpu
                                                     : MeasurementType::Cpu);

  if (isNoopRun()) {
    statistics.pushUnitAndType(typeSelector.getUnit(), typeSelector.getType());
    return TestResult::Nooped;
  }

  Sycl sycl{sycl::property::queue::enable_profiling{}};

  Timer timer;
  const size_t fillValue = 313u;

  std::unique_ptr<StreamMemoryEmbargoBenchmark> benchmark = {};
  switch (arguments.type) {
  case StreamMemoryEmbargoType::Stream_3BytesRGBtoY:
    benchmark = std::make_unique<Stream3BytesRGBToYBenchmark>(
        arguments.size, static_cast<uint8_t>(fillValue));
    break;
  case StreamMemoryEmbargoType::Stream_3BytesAlignedRGBtoY:
    benchmark = std::make_unique<Stream3BytesAlignedRGBToYBenchmark>(
        arguments.size, static_cast<uint8_t>(fillValue));
    break;
  default:
    FATAL_ERROR("Unknown StreamMemoryEmbargoType");
  }

  // Warm-up
  auto event = benchmark->run(sycl.queue);
  event.wait();

  // Benchmark
  for (auto i = 0u; i < arguments.iterations; i++) {
    timer.measureStart();
    auto event = benchmark->run(sycl.queue);
    event.wait();
    timer.measureEnd();
    if (arguments.useEvents) {
      auto startTime =
          event
              .get_profiling_info<sycl::info::event_profiling::command_start>();
      auto endTime =
          event.get_profiling_info<sycl::info::event_profiling::command_end>();
      auto timeNs = endTime - startTime;
      statistics.pushValue(std::chrono::nanoseconds(timeNs),
                           benchmark->transferSize(), typeSelector.getUnit(),
                           typeSelector.getType());
    } else {
      statistics.pushValue(timer.get(), benchmark->transferSize(),
                           typeSelector.getUnit(), typeSelector.getType());
    }
  }
  return TestResult::Success;
}

[[maybe_unused]] static RegisterTestCaseImplementation<StreamMemoryEmbargo>
    registerTestCase(run, Api::SYCL);
