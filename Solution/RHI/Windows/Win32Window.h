#pragma once
#include "../Windows/WindowsHeaders.h"


class Win32Window
{
    static unsigned long windowClassNum;
public:
    static HWND NewWindow(WNDPROC windowProcedure, HINSTANCE applicationInstance, LONG xSize, LONG ySize,
        LPCWSTR& name, HICON iconHandle = nullptr, HICON smallIconHandle = nullptr, HCURSOR cursorHandle = nullptr);

    static HWND NewChildWindow(WNDPROC windowProcedure, HINSTANCE applicationInstance,
        HWND parentHwnd, LONG x, LONG y, LONG width, LONG height, LPCWSTR name = L"EngineView");
};
