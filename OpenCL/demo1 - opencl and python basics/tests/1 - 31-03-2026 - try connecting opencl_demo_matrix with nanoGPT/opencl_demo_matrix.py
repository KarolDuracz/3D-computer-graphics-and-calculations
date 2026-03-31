import ctypes as ct
import numpy as np


# matmul
dll = ct.WinDLL(r".\opencl_demo_matrix.dll")

"""
dll.run_xw_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xw_b.restype = ct.c_int

out1 = (ct.c_int * 81)()
rc1 = dll.run_xw_b(out1)
print("run_xw_b status =", rc1)
for i in range(12):
    print(out1[i], end=" ")
print()
"""

"""
# int version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_int), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int
"""

"""
# float version
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int
"""

"""
# flaot fixed for 9 x 9 matrix
dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_int), ct.POINTER(ct.c_float),)
dll.run_xwt_b.restype = ct.c_float
"""



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

"""
# float
buffer = (ct.c_float * 5)(5.1, 3.2, 2.1, 3.8, 4.9)
pointer = ct.cast(buffer, ct.POINTER(ct.c_float))
np_array = np.ctypeslib.as_array(pointer, (5,))
c_int_array = np.ctypeslib.as_ctypes(np_array)
print(np_array)
print(type(c_int_array))
"""

"""
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
"""

"""
# flaot fixed for 9 x 9 matrix
print( " start debug ND array " )
arr_org = np.random.rand(1,48,384)
arr = arr_org.ravel()
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

print( " original array shape to restore in C ", arr_org.shape)

inp_val = ct.c_int(arr.shape[0])
out2 = (ct.c_float * 81)()
rc2 = dll.run_xwt_b(c_int_array, inp_val, out2)
print("run_xwt_b status =", rc2)
for i in range(12):
    print(out2[i], end=" ")
    print(type(out2[i]))
print()
"""

dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_float), ct.POINTER(ct.c_float),  ct.POINTER(ct.c_int), ct.POINTER(ct.c_float),)
dll.run_xwt_b.restype = ct.c_float


print( " start debug ND array " )
arr_org = np.random.rand(1,48,384)
arr = arr_org.ravel()
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

print( " original array shape to restore in C ", arr_org.shape)

inp_val = ct.c_int(arr.shape[0])
# 128 x 126 ale testuje nanoGPT
out2 = (ct.c_float * (73728))() # 81 = 9x9 | grid64 = 64x64 | nanoGPT 73728

# prepare weight
arr_orgW = np.random.rand(1536, 384)
arrW = arr_orgW.ravel()
exp_int_arrayW = np.array(arrW, dtype=np.float32)
xaW = np.ctypeslib.as_ctypes(exp_int_arrayW)
pointerW = ct.cast(xaW, ct.POINTER(ct.c_float))
np_arrayW = np.ctypeslib.as_array(pointerW, (arr.shape[0],))
c_int_arrayW = np.ctypeslib.as_ctypes(np_arrayW)

# run
rc2 = dll.run_xwt_b(c_int_array, c_int_arrayW, inp_val, out2)
    
print("run_xwt_b status =", rc2)
for i in range(36):
    print(out2[i], end=" ")
    print(type(out2[i]))
print()