<h3>TODO : How to use an array or numpy.ndarray object as an argument in C code from python code</h3>

<h3>Guide</h3>
About types and some guidelines<br />
https://numpy.org/doc/stable/reference/routines.ctypeslib.html <br />
https://docs.python.org/3/library/ctypes.html

<h3>FLOAT version</h3>


Change only the run_xwt_b function, it has 3 arguments
```
EXPORT int API run_xwt_b(float *arr, int *in, int* out)
{
    if (!out) return CL_INVALID_VALUE;
	int *temp = in;
	
	printf("%p \n", arr);
	printf("%.6f \n", arr[0]);

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
```

opencl_demo_matrix.py code for float array

```
import ctypes as ct
import numpy as np

# matmul
dll = ct.WinDLL(r".\opencl_demo_matrix.dll")

dll.run_xw_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xw_b.restype = ct.c_int

"""
# int version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_int), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int
"""

# float version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int

out1 = (ct.c_int * 12)()
rc1 = dll.run_xw_b(out1)
print("run_xw_b status =", rc1)
for i in range(12):
    print(out1[i], end=" ")
print()

#xa = ct.c_float(np.random.rand(1,48,384).tolist())

#inferred_int_array = np.array([1, 2, 3])
#c_int_array = np.ctypeslib.as_ctypes(inferred_int_array)
#print(type(c_int_array))
#print(c_int_array[:])

"""
# 29-03-2026 - dla INT dziala - to jest ok
buffer = (ct.c_int * 5)(5, 3, 2, 3, 4)
pointer = ct.cast(buffer, ct.POINTER(ct.c_int))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))
"""

# float
buffer = (ct.c_float * 5)(5.1, 3.2, 2.1, 3.8, 4.9)
pointer = ct.cast(buffer, ct.POINTER(ct.c_float))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))

inp_val = ct.c_int(100)
out2 = (ct.c_int * 12)()
rc2 = dll.run_xwt_b(c_int_array, inp_val, out2)
print("run_xwt_b status =", rc2)
for i in range(12):
    print(out2[i], end=" ")
print()
```

Result

```
> python opencl_demo_matrix.py
run_xw_b status = 0
14 6 6 6 33 13 16 16 52 20 26 26
[5.1 3.2 2.1 3.8 4.9]
<class 'c_float_Array_5'>
00000083C61C77D0
5.100000                                  // <<<<<<< here is value from arr[0]
run_xwt_b status = 0
107 11 11 4 18 27 24 11 28 43 37 18
```

<h3>INT version</h3>

opencl_demo_matrix.py

```
import ctypes as ct
import numpy as np

# matmul
dll = ct.WinDLL(r".\opencl_demo_matrix.dll")

dll.run_xw_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xw_b.restype = ct.c_int


# int version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_int), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int


"""
# float version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int
"""

out1 = (ct.c_int * 12)()
rc1 = dll.run_xw_b(out1)
print("run_xw_b status =", rc1)
for i in range(12):
    print(out1[i], end=" ")
print()

#xa = ct.c_float(np.random.rand(1,48,384).tolist())

#inferred_int_array = np.array([1, 2, 3])
#c_int_array = np.ctypeslib.as_ctypes(inferred_int_array)
#print(type(c_int_array))
#print(c_int_array[:])


# 29-03-2026 - dla INT dziala - to jest ok
buffer = (ct.c_int * 5)(5, 3, 2, 3, 4)
pointer = ct.cast(buffer, ct.POINTER(ct.c_int))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))


"""
# float
buffer = (ct.c_float * 5)(5.1, 3.2, 2.1, 3.8, 4.9)
pointer = ct.cast(buffer, ct.POINTER(ct.c_float))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))
"""

inp_val = ct.c_int(100)
out2 = (ct.c_int * 12)()
rc2 = dll.run_xwt_b(c_int_array, inp_val, out2)
print("run_xwt_b status =", rc2)
for i in range(12):
    print(out2[i], end=" ")
print()
```

opencl_demo_matrix.c - function run_xwt_b with first arguments int *arr for arrays of integers

```
EXPORT int API run_xwt_b(int *arr, int *in, int* out)
{
    if (!out) return CL_INVALID_VALUE;
	int *temp = in;
	
	printf("%p \n", arr);
	//printf("%.6f \n", arr[0]); // float version
	printf("%d %d\n", arr[0], arr[1]); // int version

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
}`
```

Result

```
> python opencl_demo_matrix.py
run_xw_b status = 0
14 6 6 6 33 13 16 16 52 20 26 26
[5 3 2 3 4]
<class 'c_long_Array_5'>
000000C02DD0B830
5 3                                  // <<<<<<< here is value from arr[0] and arr[1] for INT version
run_xwt_b status = 0
107 11 11 4 18 27 24 11 28 43 37 18
```

<h3>And one last thing here for opencl_demo_matrix.py</h3>

opencl_demo_matrix.py - For scenario with torch array like this np.random.rand(1,48,384)
This part begins with the comment ``` float with real array ```

inp_val passes the actual size of the 1D array arr.shape[0] as the second parameter of the function.

```
import ctypes as ct
import numpy as np

# matmul
dll = ct.WinDLL(r".\opencl_demo_matrix.dll")

dll.run_xw_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xw_b.restype = ct.c_int

"""
# int version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_int), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int
"""


# float version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int


out1 = (ct.c_int * 12)()
rc1 = dll.run_xw_b(out1)
print("run_xw_b status =", rc1)
for i in range(12):
    print(out1[i], end=" ")
print()

#xa = ct.c_float(np.random.rand(1,48,384).tolist())

#inferred_int_array = np.array([1, 2, 3])
#c_int_array = np.ctypeslib.as_ctypes(inferred_int_array)
#print(type(c_int_array))
#print(c_int_array[:])

"""
# 29-03-2026 - dla INT dziala - to jest ok
buffer = (ct.c_int * 5)(5, 3, 2, 3, 4)
pointer = ct.cast(buffer, ct.POINTER(ct.c_int))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))
"""


# float
buffer = (ct.c_float * 5)(5.1, 3.2, 2.1, 3.8, 4.9)
pointer = ct.cast(buffer, ct.POINTER(ct.c_float))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))


# float with real array
print( " start debug ND array " )
arr = np.random.rand(1,48,384).ravel()
exp_int_array = np.array(arr, dtype=np.float32)
print(arr.shape, type(arr))
print(arr)
xa = np.ctypeslib.as_ctypes(exp_int_array)
#buffer = (ct.c_float * arr.shape[0])(arr)
pointer = ct.cast(xa, ct.POINTER(ct.c_float))
np_array = np.ctypeslib.as_array(pointer, (arr.shape[0],))
print( " check " , len(np_array))
print(np_array)
print(40*"-")
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))

inp_val = ct.c_int(arr.shape[0])
out2 = (ct.c_int * 12)()
rc2 = dll.run_xwt_b(c_int_array, inp_val, out2)
print("run_xwt_b status =", rc2)
for i in range(12):
    print(out2[i], end=" ")
print()
```

opencl_demo_matrix.c

```
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
	printf(" print last elements %.6f \n", arr[size - 1]);

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
```

Results

```
> python opencl_demo_matrix.py
run_xw_b status = 0
14 6 6 6 33 13 16 16 52 20 26 26
[5.1 3.2 2.1 3.8 4.9]
<class 'c_float_Array_5'>
 start debug ND array
(18432,) <class 'numpy.ndarray'>
[0.47747587 0.02657393 0.6548696  ... 0.37870904 0.17126704 0.45897761]
 check  18432
[0.47747588 0.02657393 0.6548696  ... 0.37870905 0.17126705 0.4589776 ]
----------------------------------------
[0.47747588 0.02657393 0.6548696  ... 0.37870905 0.17126705 0.4589776 ]
<class 'c_float_Array_18432'>
000000C1633F11D0
print values = 0.477476 0.654870
calls from C program to check array size - 8 18432
 print last element 0.458978
run_xwt_b status = 0
18439 11 11 4 18 27 24 11 28 43 37 18
```

But I still need to check if this is what I need ( if this works) . The final piece of the puzzle is to provide a real array, for example, with size (5, 48, 384). Then, fill the kernel correctly and receive the calculations.

<h3>Summary</h3>
Looking at the last log, you can see that it's reading the array values ​​correctly for indexes 0, 2, and -1, the last element. So everything seems to be working. But here you have to manipulate Numby or Torch arrays, which are multidimensional. That's for another topic.
