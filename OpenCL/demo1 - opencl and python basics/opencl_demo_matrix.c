#define _CRT_SECURE_NO_WARNINGS
#define CL_TARGET_OPENCL_VERSION 120

#include <Windows.h>
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __declspec(dllexport)
#define API __stdcall

#define M 3
#define K 3
#define N 4

// wrong transpose calculations
#if 0
static const char* kernelSource =
"__kernel void matmul_xw_bias("
"    __global const int* X,"
"    __global const int* W,"
"    __global const int* B,"
"    __global int* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 3 || col >= 4) return;"
"    int sum = 0;"
"    for (int k = 0; k < 3; ++k) {"
"        sum += X[row * 3 + k] * W[k * 4 + col];"
"    }"
"    C[row * 4 + col] = sum + B[row * 4 + col];"
"}"
"__kernel void matmul_xwt_bias("
"    __global const int* X,"
"    __global const int* WT,"
"    __global const int* B,"
"    __global int* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 3 || col >= 4) return;"
"    int sum = 0;"
"    for (int k = 0; k < 3; ++k) {"
"        sum += X[row * 3 + k] * WT[k * 4 + col];"
"    }"
"    C[row * 4 + col] = sum + B[row * 4 + col];"
"}";
#endif

static const char* kernelSource =
"__kernel void matmul_xw_bias("
"    __global const int* X,"
"    __global const int* W,"
"    __global const int* B,"
"    __global int* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 3 || col >= 4) return;"
"    int sum = 0;"
"    for (int k = 0; k < 3; ++k) {"
"        sum += X[row * 3 + k] * W[k * 4 + col];"
"    }"
"    C[row * 4 + col] = sum + B[row * 4 + col];"
"}"
"__kernel void matmul_xwt_bias("
"    __global const int* X,"
"    __global const int* W,"
"    __global const int* B,"
"    __global int* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 3 || col >= 4) return;"
"    int sum = 0;"
"    for (int k = 0; k < 3; ++k) {"
"        sum += X[row * 3 + k] * W[col * 3 + k];"
"    }"
"    C[row * 4 + col] = sum + B[row * 4 + col];"
"}";

typedef cl_int (CL_API_CALL *PFN_clGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int (CL_API_CALL *PFN_clGetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
typedef cl_context (CL_API_CALL *PFN_clCreateContext)(const cl_context_properties*, cl_uint, const cl_device_id*,
                                                     void (CL_CALLBACK*)(const char*, const void*, size_t, void*),
                                                     void*, cl_int*);
typedef cl_command_queue (CL_API_CALL *PFN_clCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
typedef cl_program (CL_API_CALL *PFN_clCreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
typedef cl_int (CL_API_CALL *PFN_clBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
                                                 void (CL_CALLBACK*)(cl_program, void*), void*);
typedef cl_kernel (CL_API_CALL *PFN_clCreateKernel)(cl_program, const char*, cl_int*);
typedef cl_mem (CL_API_CALL *PFN_clCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
typedef cl_int (CL_API_CALL *PFN_clSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
typedef cl_int (CL_API_CALL *PFN_clEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*,
                                                         const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (CL_API_CALL *PFN_clFinish)(cl_command_queue);
typedef cl_int (CL_API_CALL *PFN_clEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*,
                                                      cl_uint, const cl_event*, cl_event*);
typedef cl_int (CL_API_CALL *PFN_clReleaseMemObject)(cl_mem);
typedef cl_int (CL_API_CALL *PFN_clReleaseKernel)(cl_kernel);
typedef cl_int (CL_API_CALL *PFN_clReleaseProgram)(cl_program);
typedef cl_int (CL_API_CALL *PFN_clReleaseCommandQueue)(cl_command_queue);
typedef cl_int (CL_API_CALL *PFN_clReleaseContext)(cl_context);
typedef cl_int (CL_API_CALL *PFN_clGetProgramBuildInfo)(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);

static void print_build_log(cl_program program, cl_device_id device, PFN_clGetProgramBuildInfo pGetBuildInfo)
{
    if (!pGetBuildInfo) return;

    size_t logSize = 0;
    if (pGetBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize) != CL_SUCCESS || logSize == 0)
        return;

    char* log = (char*)malloc(logSize + 1);
    if (!log) return;

    if (pGetBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log, NULL) == CL_SUCCESS) {
        log[logSize] = '\0';
        fprintf(stderr, "%s\n", log);
    }

    free(log);
}

static cl_int run_kernel(const char* kernelName, const int* X, const int* W, const int* B, int* C)
{
    cl_int status = CL_SUCCESS;
    HMODULE opencl = LoadLibraryW(L"OpenCL.dll");
    if (!opencl) return -1;

#define LOAD_FN(name) \
    PFN_##name p_##name = (PFN_##name)GetProcAddress(opencl, #name); \
    if (!p_##name) { status = -2; goto cleanup; }

    LOAD_FN(clGetPlatformIDs);
    LOAD_FN(clGetDeviceIDs);
    LOAD_FN(clCreateContext);
    LOAD_FN(clCreateCommandQueue);
    LOAD_FN(clCreateProgramWithSource);
    LOAD_FN(clBuildProgram);
    LOAD_FN(clCreateKernel);
    LOAD_FN(clCreateBuffer);
    LOAD_FN(clSetKernelArg);
    LOAD_FN(clEnqueueNDRangeKernel);
    LOAD_FN(clFinish);
    LOAD_FN(clEnqueueReadBuffer);
    LOAD_FN(clReleaseMemObject);
    LOAD_FN(clReleaseKernel);
    LOAD_FN(clReleaseProgram);
    LOAD_FN(clReleaseCommandQueue);
    LOAD_FN(clReleaseContext);
    LOAD_FN(clGetProgramBuildInfo);

    cl_uint numPlatforms = 0;
    status = p_clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS || numPlatforms == 0) {
        if (status == CL_SUCCESS) status = -3;
        goto cleanup;
    }

    cl_platform_id* platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * numPlatforms);
    if (!platforms) { status = -4; goto cleanup; }

    status = p_clGetPlatformIDs(numPlatforms, platforms, NULL);
    if (status != CL_SUCCESS) {
        free(platforms);
        goto cleanup;
    }

    cl_platform_id platform = platforms[0];
    free(platforms);

    cl_device_id device = NULL;
    status = p_clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (status == CL_DEVICE_NOT_FOUND) {
        status = p_clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    }
    if (status != CL_SUCCESS) goto cleanup;

    cl_context context = p_clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (!context || status != CL_SUCCESS) goto cleanup;

    cl_command_queue queue = p_clCreateCommandQueue(context, device, 0, &status);
    if (!queue || status != CL_SUCCESS) {
        p_clReleaseContext(context);
        goto cleanup;
    }

    const char* src = kernelSource;
    size_t srcLen = strlen(kernelSource);
    cl_program program = p_clCreateProgramWithSource(context, 1, &src, &srcLen, &status);
    if (!program || status != CL_SUCCESS) {
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    status = p_clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (status != CL_SUCCESS) {
        print_build_log(program, device, p_clGetProgramBuildInfo);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    cl_kernel kernel = p_clCreateKernel(program, kernelName, &status);
    if (!kernel || status != CL_SUCCESS) {
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    cl_mem xBuf = p_clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(int) * M * K, (void*)X, &status);
    cl_mem wBuf = p_clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(int) * K * N, (void*)W, &status);
    cl_mem bBuf = p_clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(int) * M * N, (void*)B, &status);
    cl_mem cBuf = p_clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                   sizeof(int) * M * N, NULL, &status);

    if (!xBuf || !wBuf || !bBuf || !cBuf || status != CL_SUCCESS) {
        if (xBuf) p_clReleaseMemObject(xBuf);
        if (wBuf) p_clReleaseMemObject(wBuf);
        if (bBuf) p_clReleaseMemObject(bBuf);
        if (cBuf) p_clReleaseMemObject(cBuf);
        p_clReleaseKernel(kernel);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    p_clSetKernelArg(kernel, 0, sizeof(cl_mem), &xBuf);
    p_clSetKernelArg(kernel, 1, sizeof(cl_mem), &wBuf);
    p_clSetKernelArg(kernel, 2, sizeof(cl_mem), &bBuf);
    p_clSetKernelArg(kernel, 3, sizeof(cl_mem), &cBuf);

    size_t global[2] = { M, N };
    status = p_clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, NULL, NULL);
    if (status == CL_SUCCESS) status = p_clFinish(queue);
    if (status == CL_SUCCESS) {
        status = p_clEnqueueReadBuffer(queue, cBuf, CL_TRUE, 0, sizeof(int) * M * N, C, 0, NULL, NULL);
    }

    p_clReleaseMemObject(xBuf);
    p_clReleaseMemObject(wBuf);
    p_clReleaseMemObject(bBuf);
    p_clReleaseMemObject(cBuf);
    p_clReleaseKernel(kernel);
    p_clReleaseProgram(program);
    p_clReleaseCommandQueue(queue);
    p_clReleaseContext(context);

cleanup:
    if (opencl) FreeLibrary(opencl);
    return status;
}

EXPORT int API run_xw_b(int* out)
{
    if (!out) return CL_INVALID_VALUE;

    int X[M * K] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    int W[K * N] = {
        1, 0, 2, 1,
        3, 1, 0, 2,
        2, 1, 1, 0
    };

    int B[M * N] = {
        1, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3
    };

    return run_kernel("matmul_xw_bias", X, W, B, out);
}

// wrong transpose - is explicit in code not in kernel
#if 0
EXPORT int API run_xwt_b(int* out)
{
    if (!out) return CL_INVALID_VALUE;

    int X[M * K] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    int WT[K * N] = {
        1, 3, 2, 0,
        2, 1, 0, 1,
        3, 2, 1, 4
    };

    int B[M * N] = {
        1, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3
    };

    return run_kernel("matmul_xwt_bias", X, WT, B, out);
}
#endif

EXPORT int API run_xwt_b(int* out)
{
    if (!out) return CL_INVALID_VALUE;

    int X[3 * 3] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    // Raw W is 4x3 here, but the flat data is the same 12 integers.
    // The kernel reads it as W.T using W[col * 3 + k].
    int W[4 * 3] = {
        1, 0, 2,
        1, 3, 1,
        0, 2, 2,
        1, 1, 0
    };

    int B[3 * 4] = {
        1, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3
    };

    return run_kernel("matmul_xwt_bias", X, W, B, out);
}