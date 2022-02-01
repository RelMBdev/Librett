//==---- dpct.hpp ---------------------------------*- C++ -*----------------==//
//
// Copyright (C) 2018 - 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//

#ifndef __DPCT_HPP__
#define __DPCT_HPP__

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wreorder-ctor"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

#include <CL/sycl.hpp>
#include <iostream>
#include <stdexcept>
#include <limits.h>

#include "atomic.hpp"
#include "device.hpp"
#include "image.hpp"
#include "kernel.hpp"
#include "memory.hpp"
#include "util.hpp"

#if defined(_MSC_VER)
#define __dpct_align__(n) __declspec(align(n))
#define __dpct_inline__ __forceinline
#else
#define __dpct_align__(n) __attribute__((aligned(n)))
#define __dpct_inline__ __inline__ __attribute__((always_inline))
#endif

#ifdef DPCT_NAMED_LAMBDA
template <class... Args> class dpct_kernel_name;
template <int Arg> class dpct_kernel_scalar;
#endif

#define DPCT_COMPATIBILITY_TEMP (600)

#define DPCT_PI_F (3.14159274101257f)
#define DPCT_PI (3.141592653589793115998)

using namespace std;
using namespace cl::sycl;

#ifdef __SYCL_DEVICE_ONLY__
extern SYCL_EXTERNAL detail::ConvertToOpenCLType_t<sycl::vec<unsigned, 4>> __spirv_GroupNonUniformBallot(int, bool) __attribute__((convergent));
#endif

extern SYCL_EXTERNAL sycl::vec<unsigned, 4> ballot(sycl::ONEAPI::sub_group, bool);

#endif // __DPCT_HPP__
