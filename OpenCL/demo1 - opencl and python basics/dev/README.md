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

