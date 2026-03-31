> [!WARNING]
> This is for informational purposes only. You won't be able to run this yourself. It needs to be improved, expanded, etc. I'm including this because the code DOES work in some sense. So that's a good start (...) THERE IS A LOT OF CODE THAT WAS TESTED ON OTHER SHAPES, a lot of garbage, but I explain below why it's there.

> [!IMPORTANT]
> There aren't full files here. There are only the ones I've modified and need to be replaced. For instructions on how to compile or run it, read below or on the first page of this repo.


<h3>HARD CODE attempt to connect opencl kernel to 1 line of code from nanoGPT for tests</h3>

Specifically, the kernel executes this one line. That is, it takes ``` x ```
and ``` self.c_fc.weight ```
and makes ``` x @ self.c_fc.weight.T ```
opencl kernel - line 168 in model.py 


https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L168

<h3>Logs and a screenshot confirming that the calculations roughly match (roughly!)</h3>
Description under the picture and logs, because it is long
<br /><br />

How to look at these logs to check if it is correct and what it is. Generally, these lines contain values ​​for x, self.c_fc.weight, a test for 10 results (see the code for where this is), etc. And finally, after the line run_xwt_b status = 0.0, there are the results returned by the OpenCl kernel, which runs on the GPU. Line by line. It starts with -1.8915358781814575 <class 'float'>'float'>

```
test [1]  tensor([ 0.3489, -0.2393,
...
show first 10 items for self.c_fc.weight.T  tensor([[ 0.0018, -0.0425
...
do test and 10 to compare  tensor([[[-1.8915,  0.6764, -
...
 arr 0.348851 -0.239333 // this is in C code to check and compare values ​​from X and self.c_fc.weight - search in the opencl_demo_matrix.c file for these lines
 arrW 0.001844 -0.025479 // this is in C code to check and compare values ​​from X and self.c_fc.weight - search in the opencl_demo_matrix.c file for these lines
```

And few results from opencl kernel 

```
run_xwt_b status = 0.0
-1.8915358781814575 <class 'float'>
0.6764249801635742 <class 'float'>
-0.7380750179290771 <class 'float'>
```

Important is line do ``` test and 10 to compare  tensor([[[-1.8915,  0.6764, ``` 
and results from ``` -1.8915358781814575 <class 'float'> ```
<b>This is what interests me most about this test. The results of what nanoGPT does vs. the openCL kernel.</b>
<br />

![dump](https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/415%20-%2031-03-2026%20-%20mix%20with%20nanoGPT.png?raw=true)

This is a log from the terminal that is displayed after starting the server with the command ``` python app2.py --out_dir=out-shakespear-char --device=cpu --port=5000 ``` 
This is the same as in the image above, but with the full log to line 177 of model.py, meaning the dead loop. It stops the program execution to check if the calculations are correct.

```
> python app2.py --out_dir=out-shakespear-char --device=cpu --port=5000
Loading model...
number of parameters: 10.67M
Model ready.
 * Serving Flask app 'app2'
 * Debug mode: off
WARNING: This is a development server. Do not use it in a production deployment. Use a production WS
GI server instead.
 * Running on all addresses (0.0.0.0)
 * Running on http://127.0.0.1:5000
 * Running on http://192.168.1.101:5000
Press CTRL+C to quit
127.0.0.1 - - [31/Mar/2026 09:05:20] "GET / HTTP/1.1" 200 -
127.0.0.1 - - [31/Mar/2026 09:05:21] "POST /api/generate HTTP/1.1" 200 -
 shape before c_fc(x)  torch.Size([1, 19, 384])
 test [1]  tensor([ 0.3489, -0.2393,  0.2740, -1.1878, -0.3370, -1.0788, -0.2295, -0.7422,
         1.4855,  0.1575])
 show first 10 items for self.c_fc.weight.T  tensor([[ 0.0018, -0.0425, -0.0025,  ..., -0.0218, -0.0
116, -0.0701],
        [-0.0255,  0.0043,  0.0201,  ...,  0.0068, -0.0075,  0.0153],
        [-0.0567, -0.0168,  0.0248,  ...,  0.0354, -0.0117,  0.0300],
        ...,
        [ 0.0488,  0.0100, -0.0272,  ..., -0.0447, -0.0529,  0.0471],
        [-0.0802,  0.0029,  0.0440,  ...,  0.0349,  0.0200, -0.0312],
        [ 0.0315, -0.0020, -0.1111,  ..., -0.0334,  0.0216,  0.0391]],
       requires_grad=True)
 do test and 10 to compare  tensor([[[-1.8915,  0.6764, -0.7381,  ..., -0.4219, -0.1768, -0.8891],
         [-1.9148,  0.2519, -1.6106,  ..., -1.3981,  0.2225, -1.2729],
         [-1.2065, -2.3809, -1.9678,  ..., -2.1653, -0.7166, -1.0996],
         ...,
         [-0.7096, -3.4413, -2.2943,  ..., -2.3620, -0.8561, -1.5815],
         [-2.4367, -1.2497, -2.2353,  ..., -2.4712,  0.0147, -1.0946],
         [-0.3073,  0.1283, -2.4641,  ..., -1.1539, -1.6690, -1.1116]]])
 start debug ND array
torch.Size([7296]) <class 'torch.Tensor'>
tensor([ 0.3489, -0.2393,  0.2740,  ..., -0.3246,  1.8459, -0.7471])
 check  7296
[ 0.34885055 -0.23933259  0.27398846 ... -0.3246468   1.8459241
 -0.7470686 ]
----------------------------------------
[ 0.34885055 -0.23933259  0.27398846 ... -0.3246468   1.8459241
 -0.7470686 ]
<class 'c_float_Array_7296'>
 original array shape to restore in C  torch.Size([1, 19, 384])
 before __job_in_thread_done = 0
 hello from run_xwt_b
000000B44331DF20
 arr 0.348851 -0.239333
 arrW 0.001844 -0.025479
print values = 0.348851 0.273988
calls from C program to check array size - 8 7296
 print last element -0.747069
 RESET state __job_in_thread_done = 0
 calst from thread worker __job_in_thread_done = 0
 show information about p_clCreateBuffer
65536 128 128
timer __rdtsc 264199284601120 264199300064978 15463858
high_res_timer 120356 120356 0.00745808
run_xwt_b status = 0.0
-1.8915358781814575 <class 'float'>
0.6764249801635742 <class 'float'>
-0.7380750179290771 <class 'float'>
-1.8948959112167358 <class 'float'>
-1.1102536916732788 <class 'float'>
1.837878942489624 <class 'float'>
-0.34985944628715515 <class 'float'>
-2.6812920570373535 <class 'float'>
-0.7673925161361694 <class 'float'>
-0.4670587480068207 <class 'float'>
0.2950928509235382 <class 'float'>
-2.0912437438964844 <class 'float'>
-0.577543318271637 <class 'float'>
-1.9744410514831543 <class 'float'>
-0.9615007638931274 <class 'float'>
-1.7984733581542969 <class 'float'>
-1.0226824283599854 <class 'float'>
-1.6665037870407104 <class 'float'>
-2.144871234893799 <class 'float'>
-0.916991114616394 <class 'float'>
0.8193063139915466 <class 'float'>
-2.595311403274536 <class 'float'>
0.45816129446029663 <class 'float'>
-1.0139403343200684 <class 'float'>
-0.6627094745635986 <class 'float'>
-1.663009762763977 <class 'float'>
-1.8636466264724731 <class 'float'>
-1.3883588314056396 <class 'float'>
-2.3773748874664307 <class 'float'>
-2.105431318283081 <class 'float'>
-0.6809305548667908 <class 'float'>
-1.3987756967544556 <class 'float'>
-0.7005298137664795 <class 'float'>
-1.376642107963562 <class 'float'>
-1.1334140300750732 <class 'float'>
-2.1947214603424072 <class 'float'>

torch.Size([1, 19, 1536])
 weights c_fc  torch.Size([1536, 384])
 test [2] x = self.c_fc(x)  tensor([-1.8915,  0.6764, -0.7381, -1.8949, -1.1103,  1.8379, -0.3499, -
2.6813,
        -0.7674, -0.4671])
 bias  self.c_fc  None
```

<h3>Description</h3>

<b>1 . OSError: exception: stack overflow - solution</b> <br /><br />
Okay, so let's get started. The program throws the error "OSError: exception: stack overflow" in such large arrays.
<br /><br />
I checked the initial steps, including using EDITBIN and checking the DUMPBIN information. It was impossible to change the program's stack settings with these tools. The remaining size was 1 MB. The solution turned out to be creating a thread (CreateThread API). In C code, this function is called "StartWork."
You can specify a STACK SIZE parameter larger than 1 MB. Look at line 44 ( and this is not 8 but 32 MB as in the comment ) in https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/opencl_demo_matrix.c#L44
<br /><br />
https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
<br /><br />
Ok, one problem solved.
<br /><br />
<b>2 . Fixed tensor sizes ( shape ) to be calculated in the kernel</b> <br /><br />

This means that the kernel is matched to the shape ``` x ```
and ``` self.c_fc.weight ```
But for testing purposes it's ok for now.
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/opencl_demo_matrix.c#L214

```
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
```

Line 787 in ``` opencl_demo_matrix.c ```
this function is called by the Python code (from model.py) - ``` run_xwt_b ```
Then, on line 793, ``` StartWork(arr, arrW, in, out); ```
Thread is called. There's a lot of junk here, code in #if <> #endif tags etc. That's why I'm writing a description
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/opencl_demo_matrix.c#L787
<br /><br />
When the thread has done its work, it returns to line 972 ``` return run_kernel_custom("matmul_xwt_bias", X, W, B, out); ```
Parameters are passed. Let's take a look at how parameters are passed between these functions.
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/opencl_demo_matrix.c#L927C2-L927C60
<br /><br />
The same applies to the first entry from the horizontal Python code into the DLL, which calls ``` EXPORT int API run_xwt_b(float *arr, float *arrW, int *in, float* out) ```
on line 787. There are 4 arguments here. Here, ``` x ```
and ``` self.c_fc.weight ```
<br /><br />
``` x ```
model.py - line 125 -> https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L125
<br /><br />
```  self.c_fc.weight ```
model.py - line 149 -> https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L149C20-L149C36
<br /><br />
Preparing arrays for the appropriate types is a topic I need to explore further. But initially, it seems to work. So, it seems like a reasonably sensible and correct implementation. Or at least, it's a starting point to test.
<br /><br />

<b>3 . Go to model.py </b><br /><br />

On line 158, it begins: ``` rc2 = dll.run_xwt_b(c_int_array, c_int_arrayW, inp_val, out2) ```
This is the input to the DLL function. Then, on lines 160-164, it reads 36 values ​​from the console, which the kernel returns to C, i.e., what it calculated, and outputs them as OUT. This is the result of the OpenCl kernel calculations. And that's what's displayed here; I just set it to 36 (it's probably 10 earlier in the demo).
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L158
<br /><br />
Then I moved what is originally calculated in nanoGPT ``` x = self.c_fc(x) ```
below because I needed ``` x ```
before executing this line.
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L168
<br /><br />
Then there are logs, and on line 178 there is a dead loop to stop for debug.
<br /><br />
THIS IS IMPORTANT. Size ``` out2 = (ct.c_float * (73728))() ```
Why is it hardcoded 73728 here? That's for this ( 1536, 384 ), which is the size of W. I think I forgot to display that in the code. But you can check it. 
<br /><br />
Then ``` out2 ```
is passed to ``` run_xwt_b ```
on line 158. At the top of the file on line 24 are ``` dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_float),) ```
defined parameters and construction of function types
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L145
<br /><br />
<b>4 . Setup for DLL in model.py </b><br /><br />
is at the top, lines 18 - 2
<br /><br />
https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/tests/1%20-%2031-03-2026%20-%20try%20connecting%20opencl_demo_matrix%20with%20nanoGPT/model.py#L18

<h3>How to run it, how I ran this test? </h3>

1. First, I'm using the "fixed" version from this repo. I run it with the command I provided there: ``` python app2.py --out_dir=out-shakespear-char --device=cpu --port=5000 ```
The web application has 1 line and 1 sample, because that's how many samples the kernel can calculate for shape x (1, 48, 384).- THIS IS IMPORTANT! - https://github.com/KarolDuracz/Machine-Learning/tree/main/1%20-%20nanoGPT%20web%20interface%20to%20generate%20samples%20from%20input/fixes
<br /><br />
So the kernel's hard code doesn't allow you to manipulate tensors like you can in pytorch. But as I wrote, I was interested in testing it out first.
<br /><br />
2. nanoGPT repo @AndrejKarpathy - https://github.com/karpathy/nanoGPT/blob/master/model.py
<br /><br />
3. If I didn't write something, the rest is explained on the first page, where I did initial tests for ``` app.py ```
, ``` opencl_demo.py ```
, ``` opencl_demo_matrix.py ```
