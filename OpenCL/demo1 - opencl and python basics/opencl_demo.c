#define _CRT_SECURE_NO_WARNINGS
#define CL_TARGET_OPENCL_VERSION 120

#include <Windows.h>
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EXPORT __declspec(dllexport)
#define API __stdcall
#define DATA_SIZE 1024

static const char* kernelSource =
"__kernel void testCoresKernel(__global float* data) {"
"    int idx = get_global_id(0);"
"    float value = (float)idx;"
"    data[idx] = sin(value) + cos(value) + exp(value);"
"}";

/* OpenCL function pointers loaded at runtime */
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
typedef cl_int (CL_API_CALL *PFN_clGetProgramBuildInfo)(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);
typedef cl_int (CL_API_CALL *PFN_clReleaseMemObject)(cl_mem);
typedef cl_int (CL_API_CALL *PFN_clReleaseKernel)(cl_kernel);
typedef cl_int (CL_API_CALL *PFN_clReleaseProgram)(cl_program);
typedef cl_int (CL_API_CALL *PFN_clReleaseCommandQueue)(cl_command_queue);
typedef cl_int (CL_API_CALL *PFN_clReleaseContext)(cl_context);

static void print_build_log(cl_program program, cl_device_id device, PFN_clGetProgramBuildInfo p_clGetProgramBuildInfo)
{
    if (!p_clGetProgramBuildInfo) return;

    size_t logSize = 0;
    if (p_clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize) != CL_SUCCESS || logSize == 0) {
        return;
    }

    char* log = (char*)malloc(logSize + 1);
    if (!log) return;

    if (p_clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log, NULL) == CL_SUCCESS) {
        log[logSize] = '\0';
        fprintf(stderr, "OpenCL build log:\n%s\n", log);
    }

    free(log);
}

EXPORT cl_int API run_opencl_demo(float* out, int count)
{
    if (!out || count <= 0) {
        return CL_INVALID_VALUE;
    }

    cl_int status = CL_SUCCESS;
    HMODULE opencl = NULL;

    PFN_clGetPlatformIDs p_clGetPlatformIDs = NULL;
    PFN_clGetDeviceIDs p_clGetDeviceIDs = NULL;
    PFN_clCreateContext p_clCreateContext = NULL;
    PFN_clCreateCommandQueue p_clCreateCommandQueue = NULL;
    PFN_clCreateProgramWithSource p_clCreateProgramWithSource = NULL;
    PFN_clBuildProgram p_clBuildProgram = NULL;
    PFN_clCreateKernel p_clCreateKernel = NULL;
    PFN_clCreateBuffer p_clCreateBuffer = NULL;
    PFN_clSetKernelArg p_clSetKernelArg = NULL;
    PFN_clEnqueueNDRangeKernel p_clEnqueueNDRangeKernel = NULL;
    PFN_clFinish p_clFinish = NULL;
    PFN_clEnqueueReadBuffer p_clEnqueueReadBuffer = NULL;
    PFN_clGetProgramBuildInfo p_clGetProgramBuildInfo = NULL;
    PFN_clReleaseMemObject p_clReleaseMemObject = NULL;
    PFN_clReleaseKernel p_clReleaseKernel = NULL;
    PFN_clReleaseProgram p_clReleaseProgram = NULL;
    PFN_clReleaseCommandQueue p_clReleaseCommandQueue = NULL;
    PFN_clReleaseContext p_clReleaseContext = NULL;

#define LOAD_FN(name) \
    do { \
        p_##name = (PFN_##name)GetProcAddress(opencl, #name); \
        if (!p_##name) { \
            fprintf(stderr, "Missing OpenCL symbol: %s\n", #name); \
            status = -2; \
            goto cleanup; \
        } \
    } while (0)

    opencl = LoadLibraryW(L"OpenCL.dll");
    if (!opencl) {
        return -1;
    }

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
    LOAD_FN(clGetProgramBuildInfo);
    LOAD_FN(clReleaseMemObject);
    LOAD_FN(clReleaseKernel);
    LOAD_FN(clReleaseProgram);
    LOAD_FN(clReleaseCommandQueue);
    LOAD_FN(clReleaseContext);

    cl_uint numPlatforms = 0;
    status = p_clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS || numPlatforms == 0) {
        if (status == CL_SUCCESS) status = -3;
        goto cleanup;
    }

    cl_platform_id* platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * numPlatforms);
    if (!platforms) {
        status = -4;
        goto cleanup;
    }

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
    if (status != CL_SUCCESS) {
        goto cleanup;
    }

    cl_context context = p_clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (!context || status != CL_SUCCESS) {
        goto cleanup;
    }

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

    cl_kernel kernel = p_clCreateKernel(program, "testCoresKernel", &status);
    if (!kernel || status != CL_SUCCESS) {
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    cl_mem buffer = p_clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * (size_t)count, NULL, &status);
    if (!buffer || status != CL_SUCCESS) {
        p_clReleaseKernel(kernel);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    status = p_clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
    if (status != CL_SUCCESS) {
        p_clReleaseMemObject(buffer);
        p_clReleaseKernel(kernel);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    size_t globalWorkSize = (size_t)count;
    status = p_clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);
    if (status != CL_SUCCESS) {
        p_clReleaseMemObject(buffer);
        p_clReleaseKernel(kernel);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    status = p_clFinish(queue);
    if (status != CL_SUCCESS) {
        p_clReleaseMemObject(buffer);
        p_clReleaseKernel(kernel);
        p_clReleaseProgram(program);
        p_clReleaseCommandQueue(queue);
        p_clReleaseContext(context);
        goto cleanup;
    }

    status = p_clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0,
                                   sizeof(float) * (size_t)count, out,
                                   0, NULL, NULL);

    p_clReleaseMemObject(buffer);
    p_clReleaseKernel(kernel);
    p_clReleaseProgram(program);
    p_clReleaseCommandQueue(queue);
    p_clReleaseContext(context);

cleanup:
    if (opencl) {
        FreeLibrary(opencl);
    }
    return status;
}