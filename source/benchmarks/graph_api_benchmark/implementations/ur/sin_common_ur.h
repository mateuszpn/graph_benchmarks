/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

class Tensor4D;
class DeviceMemoryManager;

extern const size_t a, b, c, d;
extern const size_t N;

extern size_t n_dimensions;
extern size_t global_size[];

// extern ur_kernel_handle_t kernelA, kernelS;
extern ur_kernel_handle_t *pkA, *pkS;
extern DeviceMemoryManager deviceMemMgr;

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
  void init(UrState *ur, ur_usm_pool_handle_t *pool);
  void deinit();
  float *alloc(size_t count);
  void free(void *data);

private:
  std::vector<DeviceMemoryInfo> memInfos;
  UrState *ur;
  ur_usm_pool_handle_t *pool;
  std::size_t allocCnt = 0, usedCnt = 0;
};

class Tensor4D {
public:
  Tensor4D(int A, int B, int C, int D) {
    this->A = A;
    this->B = B;
    this->C = C;
    this->D = D;
    this->data = deviceMemMgr.alloc(this->count());
  }

  std::size_t count() const { return A * B * C * D; }

  int A;
  int B;
  int C;
  int D;
  float *data;
};

Tensor4D run_model(Tensor4D &input, ur_queue_handle_t &queue,
                   int kernelIterations, bool withGraphs,
                   ur_event_handle_t *pEvent,
                   ur_exp_command_buffer_handle_t *cmdBuf);