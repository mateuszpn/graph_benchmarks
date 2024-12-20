#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SYCL_FOUND FALSE)

if(NOT SYCL_COMPILER_ROOT)
    set(SYCL_COMPILER_ROOT $ENV{CMPLR_ROOT})
    if(NOT SYCL_COMPILER_ROOT)
        message(WARNING "FindSYCL: Intel oneAPI environment not set. Ensure that Intel oneAPI is installed and use the setvars script.")
        return()
    endif()
    set(SYCL_COMPILER_ROOT ${SYCL_COMPILER_ROOT}/linux)
endif()

if(NOT SYCL_COMPILER)
    if (WIN32)
        set(SYCL_COMPILER icx)
    else()
        find_program(HAS_ICPX "icpx" NO_CACHE)
        if (HAS_ICPX)
            set(SYCL_COMPILER icpx)
        else()
            set(SYCL_COMPILER clang++)
        endif()
    endif()
endif()

set(SYCL_FLAGS "-fsycl")
if(BUILD_SYCL_WITH_CUDA)
    set(SYCL_FLAGS "${SYCL_FLAGS} -fsycl-targets=nvptx64-nvidia-cuda,spir64")
endif()

set(SYCL_FOUND TRUE)
set(SYCL_CFLAGS "${SYCL_FLAGS}" CACHE STRING "SYCL Compiler Flags")
set(SYCL_LFLAGS "${SYCL_CFLAGS} -lsycl" CACHE STRING "SYCL Linker Flags")
set(SYCL_INCLUDE_DIR "${SYCL_COMPILER_ROOT}/include" CACHE PATH "SYCL Include Directory")
set(SYCL_LIBRARY_DIR "${SYCL_COMPILER_ROOT}/lib" CACHE PATH "SYCL Library Directory")

configure_file(${PROJECT_SOURCE_DIR}/scripts/sycl-proxy.sh.in "${CMAKE_CURRENT_BINARY_DIR}/sycl-proxy.sh" @ONLY)
set(SYCL_PROXY "${CMAKE_CURRENT_BINARY_DIR}/sycl-proxy.sh" CACHE STRING "SYCL Compiler Proxy")
