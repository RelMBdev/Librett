/******************************************************************************
MIT License

Copyright (c) 2016 Antti-Pekka Hynninen
Copyright (c) 2016 Oak Ridge National Laboratory (UT-Batelle)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#include "GpuMemcpy.h"

// suppress Clang warning about it being unable to unroll a loop
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpass-failed"
#endif

#include "uniapi.h"

const int numthread = 64;

// -----------------------------------------------------------------------------------
//
// Copy using scalar loads and stores
//
template <typename T>
#if SYCL
void scalarCopyKernel(const int n, const T* data_in, T* data_out, sycl::nd_item<3> item)
#else
__global__ void scalarCopyKernel(const int n, const T* data_in, T* data_out)
#endif
{
  for (int i = threadIdx_x + blockIdx_x*blockDim_x; i < n; i += blockDim_x*gridDim_x) {
    data_out[i] = data_in[i];
  }
}

template <typename T>
void scalarCopy(const int n, const T *data_in, T *data_out, gpuStream_t& stream) {

  int numblock = (n - 1)/numthread + 1;
  // numblock = min(65535, numblock);
  // numblock = min(256, numblock);

#if SYCL
  stream->parallel_for(sycl::nd_range<3>(sycl::range<3>(1, 1, numblock) *
                                         sycl::range<3>(1, 1, numthread),
                                         sycl::range<3>(1, 1, numthread)),
                       [=](sycl::nd_item<3> item) {
                         scalarCopyKernel<T>(n, data_in, data_out, item);
                       });
#elif HIP
  hipLaunchKernelGGL(HIP_KERNEL_NAME(scalarCopyKernel<T>), dim3(numblock), dim3(numthread),
     0, stream, n, data_in, data_out);

  hipCheck(hipGetLastError());
#else // CUDA
  scalarCopyKernel<T> <<< numblock, numthread, 0, stream >>> (n, data_in, data_out);

  cudaCheck(cudaGetLastError());
#endif
}
// -----------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------
//
// Copy using vectorized loads and stores
//
template <typename T>
#if SYCL
void vectorCopyKernel(const int n, T* data_in, T* data_out, sycl::nd_item<3> item)
#else
__global__ void vectorCopyKernel(const int n, T* data_in, T* data_out)
#endif
{
  // Maximum vector load is 128 bits = 16 bytes
  const int vectorLength = 16/sizeof(T);

  int idx = threadIdx_x + blockIdx_x * blockDim_x;

  // Vector elements
  for (int i = idx; i < n/vectorLength; i += blockDim_x*gridDim_x) {
    reinterpret_cast<int4_t *>(data_out)[i] = reinterpret_cast<int4_t *>(data_in)[i];
  }

  // Remaining elements
  for (int i = idx + (n/vectorLength)*vectorLength; i < n; i += blockDim_x*gridDim_x + threadIdx_x) {
    data_out[i] = data_in[i];
  }
}

template <typename T>
void vectorCopy(const int n, T *data_in, T *data_out, gpuStream_t& stream) {

  const int vectorLength = 16/sizeof(T);

  int numblock = (n/vectorLength - 1)/numthread + 1;
  // numblock = min(65535, numblock);
  int shmemsize = 0;

#if SYCL
  stream->parallel_for(sycl::nd_range<3>(sycl::range<3>(1, 1, numblock) *
                                         sycl::range<3>(1, 1, numthread),
                                         sycl::range<3>(1, 1, numthread)),
                       [=](sycl::nd_item<3> item) {
                         vectorCopyKernel<T>(n, data_in, data_out, item);
                       });
#elif HIP
  hipLaunchKernelGGL(HIP_KERNEL_NAME(vectorCopyKernel<T>), dim3(numblock), dim3(numthread),
     shmemsize, stream, n, data_in, data_out);

  hipCheck(hipGetLastError());
#else // CUDA
  vectorCopyKernel<T> <<< numblock, numthread, shmemsize, stream >>> (n, data_in, data_out);

  cudaCheck(cudaGetLastError());
#endif
}
// -----------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------
//
// Copy using vectorized loads and stores
//
template <int numElem>
#if SYCL
void memcpyFloatKernel(const int n, float4_t *data_in, float4_t *data_out, sycl::nd_item<3> item)
#else
__global__ void memcpyFloatKernel(const int n, float4_t *data_in, float4_t *data_out)
#endif
{
  int index = threadIdx_x + numElem*blockIdx_x*blockDim_x;
  float4_t a[numElem];
  #pragma unroll
  for (int i=0; i < numElem; i++) {
    if (index + i*blockDim_x < n) a[i] = data_in[index + i*blockDim_x];
  }
  #pragma unroll
  for (int i=0; i < numElem; i++) {
    if (index + i*blockDim_x < n) data_out[index + i*blockDim_x] = a[i];
  }
}

template <int numElem>
#if SYCL
void memcpyFloatLoopKernel(const int n, float4_t *data_in, float4_t *data_out, sycl::nd_item<3> item)
#else
__global__ void memcpyFloatLoopKernel(const int n, float4_t *data_in, float4_t *data_out)
#endif
{
  for (int index=threadIdx_x + blockIdx_x*numElem*blockDim_x; index < n; index += numElem*gridDim_x*blockDim_x)
  {
    float4_t a[numElem];
    #pragma unroll
    for (int i=0; i < numElem; i++) {
      if (index + i*blockDim_x < n) a[i] = data_in[index + i*blockDim_x];
    }
    #pragma unroll
    for (int i=0; i < numElem; i++) {
      if (index + i*blockDim_x < n) data_out[index + i*blockDim_x] = a[i];
    }
  }
}

#define NUM_ELEM 2
void memcpyFloat(const int n, float *data_in, float *data_out, gpuStream_t& stream) {

  int numblock = (n/(4*NUM_ELEM) - 1)/numthread + 1;
  int shmemsize = 0;
#if SYCL
  stream->parallel_for(sycl::nd_range<3>(sycl::range<3>(1, 1, numblock) *
                                         sycl::range<3>(1, 1, numthread),
                                         sycl::range<3>(1, 1, numthread)),
                       [=](sycl::nd_item<3> item) {
                         memcpyFloatKernel<NUM_ELEM>( n/4, (float4_t *)data_in, (float4_t *)data_out, item);
                       });
#elif HIP
  hipLaunchKernelGGL(HIP_KERNEL_NAME(memcpyFloatKernel<NUM_ELEM>), dim3(numblock), dim3(numthread),
     shmemsize, stream , n/4, (float4_t *)data_in, (float4_t *)data_out);

  hipCheck(hipGetLastError());
#else // CUDA
  memcpyFloatKernel<NUM_ELEM> <<< numblock, numthread, shmemsize, stream >>>
  (n/4, (float4_t *)data_in, (float4_t *)data_out);

  // int numblock = 64;
  // int shmemsize = 0;
  // memcpyFloatLoopKernel<NUM_ELEM> <<< numblock, numthread, shmemsize, stream >>>
  // (n/4, (float4 *)data_in, (float4 *)data_out);

  cudaCheck(cudaGetLastError());
#endif
}
// -----------------------------------------------------------------------------------

// Explicit instances
template void scalarCopy<int>(const int n, const int* data_in, int* data_out, gpuStream_t& stream);
template void scalarCopy<long long int>(const int n, const long long int* data_in, long long int* data_out,
                                        gpuStream_t& stream);
template void vectorCopy<int>(const int n, int* data_in, int* data_out, gpuStream_t& stream);
template void vectorCopy<long long int>(const int n, long long int* data_in, long long int* data_out,
                                        gpuStream_t& stream);
void memcpyFloat(const int n, float* data_in, float* data_out, gpuStream_t& stream);
