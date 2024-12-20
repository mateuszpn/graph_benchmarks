/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
class Tensor4D;

extern const size_t N;

#define random_float() (rand() / double(RAND_MAX) * 20. - 10.)

struct DeviceMemoryInfo {
  float *data; // float is enough for the benchmark
  size_t count;
  bool used;
};

class DeviceMemoryManager {
public:
  DeviceMemoryManager() {}
  ~DeviceMemoryManager() {}
  void init(LevelZero *lz);
  void deinit();
  float *alloc(size_t count);
  void free(void *data, const std::string name);

private:
  std::vector<DeviceMemoryInfo> memInfos;
  LevelZero *levelzero;
  std::size_t allocCnt = 0, usedCnt = 0;
};

extern DeviceMemoryManager deviceMemMgr;

class Tensor4D {
public:
  Tensor4D(int A, int B, int C, int D) {
    this->A = A;
    this->B = B;
    this->C = C;
    this->D = D;
    this->data = deviceMemMgr.alloc(count());
  }

  std::size_t count() const { return A * B * C * D; }

  int A;
  int B;
  int C;
  int D;
  float *data;
};

Tensor4D run_model(Tensor4D &input, int kernelIterations,
                   ze_kernel_handle_t &kernelA, ze_kernel_handle_t &kernelS,
                   bool withGraphs, ze_command_queue_handle_t &cmdQueue,
                   ze_command_list_handle_t &cmdList);