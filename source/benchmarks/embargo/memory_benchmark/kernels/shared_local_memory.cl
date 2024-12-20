/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they were
 * provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit this
 * software or the related documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no express or
 * implied warranties, other than those that are expressly stated in the License.
 */

__kernel void slm_alloc_size(__global int *outBuffer, __local int *slm, int doWrite) {
    int localId = get_local_id(0);
    slm[localId] = localId;
    barrier(CLK_LOCAL_MEM_FENCE);

    int sum = 0;
    for(int i = 0; i < get_local_size(0); i++){
        sum += slm[i];
    }

    if(doWrite > 0 || get_global_id(0) == 0){
        outBuffer[get_global_id(0)] = sum;
    }
}
