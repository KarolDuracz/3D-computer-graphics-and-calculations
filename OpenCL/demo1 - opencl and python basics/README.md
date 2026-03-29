<h3>Demo 1 - OpenCL program runs from Python</h3>

Python gives ability to run C code from within python https://docs.python.org/3/library/ctypes.html
<br /><br />
<h4>Setup</h4>
I choose for compilation x64 CL.exe (insted of x86)

```
C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Visual Studio 2019\Visual Studio Tools\VC\x64 Native Tools Command Prompt for VS 2019
Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30154 for x64
Microsoft (R) Incremental Linker Version 14.29.30154.0
```

for me CL\cl.h header is from path for linker 
```
/I"C:\Users\kdhome\OpenCL-SDK\clGPU-master\clGPU-master\common\intel_ocl_icd\windows\include"
```

But cl.h can be found at least on the official KhronosGroup repo
```
https://github.com/KhronosGroup/OpenCL-Headers/tree/8a97ebc88daa3495d6f57ec10bb515224400186f/CL
```

OpenCL.dll exists in System32 folder. I copied it there, that's why the ``` path HMODULE opencl = LoadLibraryW(L"OpenCL.dll");```
doesn't return errors and finds this DLL
<br /><br />
And the MSVC but only the CL.exe console tool itself, i.e. the compiler and linker.

<h4>Test 1 - run.bat</h4>

.bat files, e.g. run.bat, have ready-made commands inserted to compile this ``` cl /LD /O2 calc.c /Fe:calc.dll ``` . Basic math operations to start the tests.
<br /><br />
Run via command ``` python app.py ``` and the result is 

```
5
20
```

This is the result we expect. Looks ok.

<h4>Test 2 - run_opencl.bat</h4>

Ok, time for opencl. Kernels are loaded as strings https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/opencl_demo.c#L15
<br /><br />

Runs

```
pyhon opencl_demo.py
```

```
import ctypes as ct

dll = ct.WinDLL(r".\opencl_demo.dll")  # stdcall / WinDLL

dll.run_opencl_demo.argtypes = (ct.POINTER(ct.c_float), ct.c_int)
dll.run_opencl_demo.restype = ct.c_int

N = 1024
out = (ct.c_float * N)()

rc = dll.run_opencl_demo(out, N)
print("status =", rc)

for i in range(10):
    print(i, out[i])
```

Results

```
status = 0
0 2.0
1 4.100054740905762
2 7.882206916809082
3 19.236663818359375
4 53.18770217895508
5 147.73789978027344
6 404.10955810546875
7 1098.0440673828125
8 2981.8017578125
9 8102.5849609375
```

<h4>Test 3 - MATMUL - run_opencl_matrix.bat</h4>

I used integers instead of floats to make it easier to check if it works correctly initially. There are 2 kernels 
 https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/opencl_demo_matrix.c#L50 .  The first one does

```
x = torch.tensor([1, 2, 3, 4, 5, 6, 7, 8, 9]).view(3,3)
x
tensor([[1, 2, 3],
        [4, 5, 6],
        [7, 8, 9]])
w = torch.tensor([1, 0, 2, 1, 3, 1, 0, 2, 2, 1, 1, 0]).view(3,4)
w
tensor([[1, 0, 2, 1],
        [3, 1, 0, 2],
        [2, 1, 1, 0]])
b = torch.tensor([1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3]).view(3,4)
b
tensor([[1, 1, 1, 1],
        [2, 2, 2, 2],
        [3, 3, 3, 3]])
x @ w + b
tensor([[14,  6,  6,  6],
        [33, 13, 16, 16],
        [52, 20, 26, 26]])
```

Second with Transpose - x @ w.T+ b

```
x = torch.tensor([1, 2, 3, 4, 5, 6, 7, 8, 9]).view(3,3)
x
tensor([[1, 2, 3],
        [4, 5, 6],
        [7, 8, 9]])
w = torch.tensor([1, 0, 2, 1, 3, 1, 0, 2, 2, 1, 1, 0]).view(4,3)
w
tensor([[1, 0, 2],
        [1, 3, 1],
        [0, 2, 2],
        [1, 1, 0]])
b = torch.tensor([1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3]).view(3,4)
b
tensor([[1, 1, 1, 1],
        [2, 2, 2, 2],
        [3, 3, 3, 3]])
x @ w.T+ b
tensor([[ 8, 11, 11,  4],
        [18, 27, 24, 11],
        [28, 43, 37, 18]])
```

Runs

```
python opencl_demo_matrix.py
```

Results

```
run_xw_b status = 0
14 6 6 6 33 13 16 16 52 20 26 26 // x @ w + b
run_xwt_b status = 0
8 11 11 4 18 27 24 11 28 43 37 18 // x @ w.T+ b
```

Pictures from test 3

![dump](https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/transpose%20kernel%20calculations.png?raw=true)

![dump](https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenCL/demo1%20-%20opencl%20and%20python%20basics/transpose%20test.png?raw=true)


