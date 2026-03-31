#define _CRT_SECURE_NO_WARNINGS
#define CL_TARGET_OPENCL_VERSION 120

#include <Windows.h>
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __declspec(dllexport)
#define API __stdcall

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


// jeśli modyfikujesz coś PAMIETAJ ŻE TE ZMIENNE SĄ w run_kernel_custom
#define M 128 // 9
#define K 128 // 9
#define N 128 // 9

#include <windows.h>

DWORD WINAPI WorkerThread(LPVOID lpParameter);

typedef struct WorkArgs {
    float *arr;
	float *arrW;
    int   *in;
    float *out;
} WorkArgs;

/*__declspec(dllexport)*/ EXPORT void StartWork(float *arr, float *arrW, int *in, float *out)
{
    WorkArgs *args = (WorkArgs *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WorkArgs));
    if (!args) return;

    args->arr = arr;
	args->arrW = arrW;
    args->in  = in;
    args->out = out;

    HANDLE h = CreateThread(
        NULL,
        32 * 1024 * 1024,   // 8 MB stack reserve
        WorkerThread,
        args,
        0,
        NULL
    );

    if (h) {
        CloseHandle(h);
    } else {
        HeapFree(GetProcessHeap(), 0, args);
    }
}

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

/* kopia tego co działa niżej */
/*
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

*/

	/* 9 x 9 matrix test with floats - pass */

/* 
static const char* kernelSource =
"__kernel void matmul_xwt_bias("
"    __global const float* X,"
"    __global const float* W,"
"    __global const float* B,"
"    __global float* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 9 || col >= 9) return;"
"    float sum = 0;"
"    for (int k = 0; k < 9; ++k) {"
"        sum += X[row * 9 + k] * W[col * 9 + k];"
"    }"
"    C[row * 9 + col] = sum + B[row * 9 + col];"
"}";
*/

	/* dynamic size (1, 48, xxx) */

#if 0
static const char* kernelSource =
"__kernel void matmul_xwt_bias("
"    __global const float* X,"
"    __global const float* W,"
"    __global const float* B,"
"    __global float* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 9 || col >= 9) return;"
"    float sum = (float)row;"
"    for (int k = 0; k < 9; ++k) {"
//"        sum += X[row * 9 + k] * W[col * 9 + k];"
"			sum = X[col];"
//"			row = row + 2;"

"			C[row] = col;"
"    }"
//"    C[row * 9 + col] = sum + B[row * 9 + col];"
//"	C[row * 9 + col] = sum;"
"}";

#endif

/*
static const char* kernelSource =
"__kernel void matmul_xwt_bias("
"    __global const float* X,"
"    __global const float* W,"
"    __global const float* B,"
"    __global float* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 128 || col >= 128) return;"
"    float sum = 0;"
//"		for (int loop = 0; loop < 1000; loop++) {"
"    for (int k = 0; k < 128; ++k) {"
//"        sum += X[row * 128 + k] * X[col * 128 + k];"
//"		for (int loop = 0; loop < 100; loop++) {"
"        sum += X[row * 128 + k] * X[k * 128  + col];"
"	}"
//"		sum = sin(sum);"
//"    }"
//"	}"
"    C[row * 128 + col] = sum + B[row * 128 + col];"
"}";
*/

/*
static const char* kernelSource =
"__kernel void matmul_xwt_bias("
"    __global const float* X,"
"    __global const float* W,"
"    __global const float* B,"
"    __global float* C) {"
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
"    if (row >= 128 || col >= 128) return;"
"    float sum = 0;"
"    for (int k = 0; k < 128; ++k) {"
//"        sum += X[row * 128 + k] * X[col * 128 + k];"
"        sum += X[row * 128 + k] * X[k * 128  + col];"
"	}"
"    C[row * 128 + col] = sum + B[row * 128 + col];"
"}";
*/

/* kernel dla MLP nanoGPT - pierwszy test 31.03.2026 */
static const char* kernelSource =
"__kernel void matmul_xwt_bias("
"    __global const float* X, "
"    __global const float* W, "
"    __global const float* B, "
"    __global float* C) {"
"    const int ROWS = 48;"
"    const int IN_K  = 384;"
"    const int COLS  = 1536;"
""
"    int row = get_global_id(0);"
"    int col = get_global_id(1);"
""
"    if (row >= ROWS || col >= COLS) return;"
""
"    float sum = 0.0f;"
"    for (int k = 0; k < IN_K; ++k) {"
"        sum += X[row * IN_K + k] * W[col * IN_K + k];"
"    }"
""
"    C[row * COLS + col] = sum + B[row * COLS + col];"
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
	
		printf(" show information about p_clCreateBuffer \n");
		printf("%lld %d %d\n", (sizeof(int) * M * K), M, K);

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

static double high_res_timer() {
	LARGE_INTEGER nowt2;
	LARGE_INTEGER f2; 
	QueryPerformanceCounter(&nowt2);
	QueryPerformanceFrequency(&f2);
	double dTime = (double)(nowt2.QuadPart) / (double)(f2.QuadPart);
	return dTime;
}

// needs to change also kernel main function from CONST INT* --> const float*
static cl_int run_kernel_custom(const char* kernelName, const float* X, const float* W, const float* B, float* C)
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
                                   sizeof(float) * M * K, (void*)X, &status);
    cl_mem wBuf = p_clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * K * N, (void*)W, &status);
    cl_mem bBuf = p_clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * M * N, (void*)B, &status);
    cl_mem cBuf = p_clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                   sizeof(float) * M * N, NULL, &status);
	
		printf(" show information about p_clCreateBuffer \n");
		printf("%lld %d %d\n", (sizeof(float) * M * K), M, K);

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

	// measure kernel execution
	unsigned __int64 start = __rdtsc();
	unsigned __int64 end;
	double __start = high_res_timer();
	/////////////////////////////////////////

    size_t global[2] = { M, N };
    status = p_clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, NULL, NULL);
    if (status == CL_SUCCESS) status = p_clFinish(queue);
    if (status == CL_SUCCESS) {
        status = p_clEnqueueReadBuffer(queue, cBuf, CL_TRUE, 0, sizeof(float) * M * N, C, 0, NULL, NULL);
    }

	/////////////////////////////////////////
    end = __rdtsc();
	printf("timer __rdtsc %I64d %I64d %I64d  \n", start, end, (end - start));
	double __end = high_res_timer();
	printf("high_res_timer %g %g %g \n", __start, __end, (__end - __start));
	/////////////////////////////////////////
	
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

#if 0
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
#endif

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

// ok to jest demo z github - stan na 30-03-026 - 00:35
// niżej jest kopia tej funkcji tylko już chcę robić operacje na dużych tablicach

#if 0
EXPORT int API run_xwt_b(float *arr, int *in, int* out)
//EXPORT int API run_xwt_b(int *arr, int *in, int* out)
{
    if (!out) return CL_INVALID_VALUE;
	int *temp = in;
	
	printf("%p \n", arr);
	//printf("%.6f \n", arr[0]); // float version
	//printf("%d %d\n", arr[0], arr[1]); // int version
	printf("print values = %.6f %.6f \n", arr[0], arr[2]);
	
	// #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
	// check array size
	printf("calls from C program to check array size - %I64u %d \n", sizeof(arr), *in);
	
	int size = *in; 
	printf(" print last element %.6f \n", arr[size - 1]);

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

    //int B[3 * 4] = {
    //    1, 1, 1, 1,
    //    2, 2, 2, 2,
    //    3, 3, 3, 3
    //};
	
	int B[3 * 4] = {
        *temp, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3
    };

    return run_kernel("matmul_xwt_bias", X, W, B, out);
}
#endif

// keep but probably don't needed __job_in_thread_done
volatile int __job_in_thread_done = 0;
float X[1 * 48 * 384];
float W[1536 * 384];
float B[ 48 * 1536];


DWORD WINAPI WorkerThread(LPVOID lpParameter)
{
    WorkArgs *args = (WorkArgs *)lpParameter;

    float *arr = args->arr;
	float *arrW = args->arrW;
    int   *in  = args->in;
    float *out = args->out;

    // heavy native work here
    // use arr, in, out
	
	// needs to go arrays from arr, arrW
	
	printf(" arr %.6f %.6f \n", arr[0], arr[1]);
	printf(" arrW %.6f %.6f \n", arrW[0], arrW[1]);
	
	// create B
	int val_b = 48 * 1536;
	float bal2[ 48 * 1536] = { 0 };
	int f, o;
	int h = 0;
	for (f = 0; f < val_b; f++) {
		bal2[h] = 0;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}
	
	int val_x = 1 * 48 * 384;
	int val_w = 1536 * 384;
	
	memcpy(X, arr, (val_x * sizeof(float)));
	memcpy(W, arrW, (val_w * sizeof(float)));
	memcpy(B, bal2, (val_b * sizeof(float)));
	
	// this was test - just to check if programs gives back out results from kernel to python sample part
#if 0
	// create X
	int val_x = 1 * 48 * 384;
	float bal[1 * 48 * 384] = { 0 };
	int f, o;
	int h = 0;
	for (f = 0; f < val_x; f++) {
		bal[h] = (rand() % 20) * 0.1;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}
	
	// create W
	int val_w = 1536 * 384;
	float bal1[1536 * 384] = { 0 };
	f, o;
	h = 0;
	for (f = 0; f < val_w; f++) {
		bal1[h] = (rand() % 20) * 0.1;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}

	// create B
	int val_b = 48 * 1536;
	float bal2[ 48 * 1536] = { 0 };
	f, o;
	h = 0;
	for (f = 0; f < val_b; f++) {
		bal2[h] = 0;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}
	
	
	memcpy(X, bal, (val_x * sizeof(float)));
	memcpy(W, bal1, (val_w * sizeof(float)));
	memcpy(B, bal2, (val_b * sizeof(float)));
#endif
	

	printf(" calst from thread worker __job_in_thread_done = %d \n", __job_in_thread_done);
	__job_in_thread_done = 1;
    
}


EXPORT int API run_xwt_b(float *arr, float *arrW, int *in, float* out)
//EXPORT int API run_xwt_b(int *arr, int *in, int* out)
{
	
	if (__job_in_thread_done == 0) {
		printf(" before __job_in_thread_done = %d \n", __job_in_thread_done);
		StartWork(arr, arrW, in, out);
	}
	
	printf( " hello from run_xwt_b \n");
	
    if (!out) return CL_INVALID_VALUE;
	int *temp = in;
	
	printf("%p \n", arr);
	//printf("%.6f \n", arr[0]); // float version
	//printf("%d %d\n", arr[0], arr[1]); // int version
	printf("print values = %.6f %.6f \n", arr[0], arr[2]);
	
	// #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
	// check array size
	printf("calls from C program to check array size - %I64u %d \n", sizeof(arr), *in);
	
	int size = *in; 
	printf(" print last element %.6f \n", arr[size - 1]);
	
//#define __bazowyKod
#ifdef __bazowyKod

#define ARR_SIZE 128

#if 1
	// generate random numbers 2d array
	float bal[ARR_SIZE * ARR_SIZE] = { 0 };
	int f, o;
	int h = 0;
	for (f = 0; f < ARR_SIZE * ARR_SIZE; f++) {
		bal[h] = (rand() % 20) * 0.1;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}

	printf("BAL %.6f \n", bal[(ARR_SIZE * ARR_SIZE) - 1]);
#endif

	float X[ARR_SIZE * ARR_SIZE];
	float W[ARR_SIZE * ARR_SIZE];
	float B[ARR_SIZE * ARR_SIZE];

	memcpy(X, bal, (ARR_SIZE * ARR_SIZE * sizeof(float)));
	memcpy(W, bal, (ARR_SIZE * ARR_SIZE * sizeof(float)));
	memcpy(B, bal, (ARR_SIZE * ARR_SIZE * sizeof(float)));

	printf("BAL %.6f %.6f\n", X[100], bal[100]);
#endif
	
#if 0
	// create X
	int val_x = 1 * 48 * 384;
	float bal[1 * 48 * 384] = { 0 };
	int f, o;
	int h = 0;
	for (f = 0; f < val_x; f++) {
		bal[h] = (rand() % 20) * 0.1;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}
	
	// create W
	int val_w = 1536 * 384;
	float bal1[1536 * 384] = { 0 };
	f, o;
	h = 0;
	for (f = 0; f < val_w; f++) {
		bal1[h] = (rand() % 20) * 0.1;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}

	// create B
	int val_b = 48 * 1536;
	float bal2[ 48 * 1536] = { 0 };
	f, o;
	h = 0;
	for (f = 0; f < val_b; f++) {
		bal2[h] = 0;
		//printf("BAL %.6f \n", bal[h]);
		//break;
		h += 1;
	}
	
	float X[1 * 48 * 384];
	float W[1536 * 384];
	float B[ 48 * 1536];

	memcpy(X, bal, (val_x * sizeof(float)));
	memcpy(W, bal1, (val_w * sizeof(float)));
	memcpy(B, bal2, (val_b * sizeof(float)));
#endif

#if 0
    float X[9 * 9] = {
        0., 0., 1., 2., 2., 2., 0., 0., 2., 1., 0., 2., 2., 0., 0., 1., 1., 0.,
        0., 2., 2., 0., 1., 0., 2., 1., 1., 0., 0., 2., 1., 2., 0., 1., 0., 1.,
        0., 1., 0., 2., 0., 2., 1., 0., 0., 1., 2., 0., 2., 1., 1., 1., 2., 1.,
        2., 0., 1., 1., 1., 2., 2., 1., 1., 0., 0., 2., 1., 1., 1., 2., 2., 1.,
        1., 2., 0., 0., 1., 0., 1., 0., 1.
    };

    // Raw W is 4x3 here, but the flat data is the same 12 integers.
    // The kernel reads it as W.T using W[col * 3 + k].
    float W[9 * 9] = {
        0., 0., 1., 2., 2., 2., 0., 0., 2., 1., 0., 2., 2., 0., 0., 1., 1., 0.,
        0., 2., 2., 0., 1., 0., 2., 1., 1., 0., 0., 2., 1., 2., 0., 1., 0., 1.,
        0., 1., 0., 2., 0., 2., 1., 0., 0., 1., 2., 0., 2., 1., 1., 1., 2., 1.,
        2., 0., 1., 1., 1., 2., 2., 1., 1., 0., 0., 2., 1., 1., 1., 2., 2., 1.,
        1., 2., 0., 0., 1., 0., 1., 0., 1.
    };

    //int B[3 * 4] = {
    //    1, 1, 1, 1,
    //    2, 2, 2, 2,
    //    3, 3, 3, 3
    //};
	
	float B[9 * 9] = {
        0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
        0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
        0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
        0., 0., 0., 0., 0., 0., 0., 0., 0.
    };
#endif

	printf(" RESET state __job_in_thread_done = %d \n", __job_in_thread_done);
	__job_in_thread_done = 0; // to prevent if sth calls more that once, just reset variable to 0 again
    
	// go to execute kernel from source
	return run_kernel_custom("matmul_xwt_bias", X, W, B, out);
}