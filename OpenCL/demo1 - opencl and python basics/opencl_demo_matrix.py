import ctypes as ct

# matmul
dll = ct.WinDLL(r".\opencl_demo_matrix.dll")

dll.run_xw_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xw_b.restype = ct.c_int

dll.run_xwt_b.argtypes = (ct.POINTER(ct.c_int),)
dll.run_xwt_b.restype = ct.c_int

out1 = (ct.c_int * 12)()
rc1 = dll.run_xw_b(out1)
print("run_xw_b status =", rc1)
for i in range(12):
    print(out1[i], end=" ")
print()

out2 = (ct.c_int * 12)()
rc2 = dll.run_xwt_b(out2)
print("run_xwt_b status =", rc2)
for i in range(12):
    print(out2[i], end=" ")
print()