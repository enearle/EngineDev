#include "Win32Window.h"

#include <algorithm>
#include <vector>
#include "Win32ErrorHandler.h"
#include "../GraphicsSettings.h"

unsigned long Win32Window::windowClassNum = 0;

static BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM monitors)
{
    reinterpret_cast<std::vector<HMONITOR>*>(monitors)->push_back(monitor);
    return TRUE;
}

// Monitors are numbered with the primary first, then from left to right.
bool Win32Window::FindMonitorWorkArea(int monitorIndex, RECT& workArea)
{
    if (monitorIndex < 0)
        return false;

    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    std::vector<MONITORINFO> infos;
    for (HMONITOR monitor : monitors)
    {
        MONITORINFO info = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(monitor, &info))
            infos.push_back(info);
    }
    std::sort(infos.begin(), infos.end(), [](const MONITORINFO& a, const MONITORINFO& b)
    {
        bool isAPrimary = a.dwFlags & MONITORINFOF_PRIMARY;
        bool isBPrimary = b.dwFlags & MONITORINFOF_PRIMARY;
        return isAPrimary != isBPrimary ? isAPrimary : a.rcMonitor.left < b.rcMonitor.left;
    });

    if (monitorIndex >= static_cast<int>(infos.size()))
        return false;
    workArea = infos[monitorIndex].rcWork;
    return true;
}

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
    LONG borderWidth = windowRect.right - windowRect.left - xSize;
    LONG borderHeight = windowRect.bottom - windowRect.top - ySize;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    RECT workArea = {};
    if (FindMonitorWorkArea(GRAPHICS_SETTINGS.Monitor, workArea))
    {
        float fit = (std::min)({ 1.0f, static_cast<float>(workArea.right - workArea.left - borderWidth) / xSize,
                                 static_cast<float>(workArea.bottom - workArea.top - borderHeight) / ySize });
        xSize = static_cast<LONG>(xSize * fit);
        ySize = static_cast<LONG>(ySize * fit);
        x = workArea.left;
        y = workArea.top;
    }

    return CreateWindowEx(0, instanceName.c_str(), name, WS_OVERLAPPEDWINDOW, x, y,
        xSize + borderWidth, ySize + borderHeight, nullptr, nullptr, applicationInstance, nullptr);
}

HWND Win32Window::NewChildWindow(WNDPROC windowProcedure, HINSTANCE applicationInstance,
    HWND parentHwnd, LONG x, LONG y, LONG width, LONG height, LPCWSTR name)
{
    std::wstring instanceName = std::wstring(name) + L'_' + std::to_wstring(windowClassNum++);

    WNDCLASSEX wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = windowProcedure;
    wc.hInstance     = applicationInstance;
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = instanceName.c_str();

    if (!RegisterClassEx(&wc))
    {
        Win32ErrorHandler::ErrorMessageW(L"RegisterClassEx failed for " + instanceName + L".");
        return nullptr;
    }

    return CreateWindowEx(0, instanceName.c_str(), name,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height,
        parentHwnd, nullptr, applicationInstance, nullptr);
}
