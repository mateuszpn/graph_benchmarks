/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void kernel_sin(__global float * dest, __global float * source, int size) {
    int id = get_global_id(0);
    if (id < size) {
        dest[id] = sin(source[id]);
    }
}