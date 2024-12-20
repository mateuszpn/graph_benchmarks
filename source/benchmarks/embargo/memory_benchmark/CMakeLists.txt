#
# INTEL CONFIDENTIAL
#
# Copyright (C) 2019-2024 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials,
# and your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise,
# you may not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.

add_benchmark(memory_benchmark_embargo ocl l0 sycl all)

get_property(ALL_APIS GLOBAL PROPERTY APIS)
foreach(API ${ALL_APIS})
    # UR not supported yet
    if("${API}" STREQUAL "ur")
        continue()
    endif()
    set(TARGET_NAME "memory_benchmark_embargo_${API}")
    set(TARGET_LOCATION "memory_benchmark_embargo_${API}_LOCATION")
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/embargo")
    set_property(GLOBAL PROPERTY ${TARGET_LOCATION} "embargo")
endforeach()
