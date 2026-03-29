from ctypes import WinDLL, c_int

dll = WinDLL(r".\calc.dll")

dll.add.argtypes = (c_int, c_int)
dll.add.restype = c_int

dll.mul.argtypes = (c_int, c_int)
dll.mul.restype = c_int

print(dll.add(2, 3))   # 5
print(dll.mul(4, 5))   # 20