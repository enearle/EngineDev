#pragma once
#include <windows.h>

class Win32Window
{
    static u_long windowClassNum;   
public:
    static HWND NewWindow(WNDPROC windowProcedure, HINSTANCE applicationInstance, LONG xSize, LONG ySize,
        LPCWSTR& name, HICON iconHandle = nullptr, HICON smallIconHandle = nullptr, HCURSOR cursorHandle = nullptr);
    
};
