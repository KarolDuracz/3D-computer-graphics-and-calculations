#include <windows.h>

__declspec(dllexport) int __stdcall add(int a, int b) {
    return a + b;
}

__declspec(dllexport) int __stdcall mul(int a, int b) {
    return a * b;
}

// Optional, but common in DLLs
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved) {
    return TRUE;
}