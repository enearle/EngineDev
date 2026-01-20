#include "Win32Window.h"
#include <windowsx.h>
#include "Win32ErrorHandler.h"

u_long Win32Window::windowClassNum = 0;

HWND Win32Window::NewWindow(WNDPROC windowProcedure, HINSTANCE applicationInstance, LONG xSize, LONG ySize,
                                    LPCWSTR& name, HICON iconHandle, HICON smallIconHandle, HCURSOR cursorHandle)
{
    std::wstring instanceName = std::wstring(name) + L'_' + std::to_wstring(windowClassNum++);
    
    WNDCLASSEX newWindow;
    newWindow.style         = CS_HREDRAW | CS_VREDRAW;
    newWindow.lpfnWndProc   = windowProcedure; 
    newWindow.cbClsExtra    = 0;
    newWindow.cbWndExtra    = 0;
    newWindow.cbSize        = sizeof(WNDCLASSEX);
    newWindow.hInstance     = applicationInstance;
    newWindow.hIcon         = iconHandle ? iconHandle : LoadIcon(0, IDI_APPLICATION);
    newWindow.hIconSm       = smallIconHandle ? smallIconHandle : LoadIcon(0, IDI_APPLICATION);
    newWindow.hCursor       = cursorHandle ? cursorHandle : LoadCursor(0, IDC_ARROW);
    newWindow.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    newWindow.lpszMenuName  = 0;
    newWindow.lpszClassName = instanceName.c_str();

    if (!RegisterClassEx(&newWindow))
    {
        Win32ErrorHandler::ErrorMessageW(L"RegisterClassEx failed for " + instanceName + L".");
        return nullptr;
    }

    RECT windowRect = { 0, 0, xSize, ySize};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    xSize = windowRect.right - windowRect.left;
    ySize = windowRect.bottom - windowRect.top;
    
    return CreateWindowEx(0, instanceName.c_str(), name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, xSize, ySize, nullptr, nullptr, applicationInstance,
        nullptr);
}
