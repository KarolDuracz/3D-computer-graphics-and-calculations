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