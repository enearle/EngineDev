#include "Window.h"

#include <windowsx.h>

#include "Input/InputState.h"
#include "Windows/Win32ErrorHandler.h"
#include "Windows/Win32Window.h"

using namespace  Win32ErrorHandler;

std::map<HWND, Window*> Window::WindowRegistry;

Window::Window(LPCWSTR windowName, WindowType windowType, LONG xSize, LONG ySize)
    : WindowHandle(nullptr), InstanceHandle(nullptr)
{
    
    switch (windowType)
    {
    case Win32:
        InstanceHandle = GetModuleHandle(nullptr);
        WindowHandle = Win32Window::NewWindow(StaticWindowProcedure, InstanceHandle, xSize, ySize, windowName);
        RegisterWindow(WindowHandle, this);
        break;
    default:
        ErrorMessageW(L"Window::Window() called with invalid window type.");
        throw std::runtime_error("Failed to construct window.");
        break;
    }
}

LRESULT Window::WindowProcedure(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ENTERSIZEMOVE:
        Resizing = true;
        return 0;

    case WM_EXITSIZEMOVE:
        Resizing = false;
        OnResize(Width, Height);
        return 0;
    
    case WM_SIZE:
        {
            Width = LOWORD(lParam);
            Height = HIWORD(lParam);
            switch (wParam)
            {
            case SIZE_MAXIMIZED:
                OnMaximize();
                break;
                
            case SIZE_MINIMIZED:
                OnMinimize();
                break;

            case SIZE_RESTORED:
                OnResize(Width, Height);
                break;
            }
        }
        return 0;
        
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
        
    case WM_DESTROY:
        WindowHandle = nullptr;
        return 0;

    case WM_KEYDOWN:
        InputState::GetInstance().SetKeyDown(wParam);
        return 0;

    case WM_KEYUP:
        InputState::GetInstance().SetKeyUp(wParam);
        if(wParam == VK_ESCAPE)
            PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
        InputState::GetInstance().SetMouseButtonDown(0);
        return 0;

    case WM_LBUTTONUP:
        InputState::GetInstance().SetMouseButtonUp(0);
        return 0;

    case WM_MOUSEMOVE:
        InputState::GetInstance().SetMousePosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
        
    default:
        return DefWindowProc(WindowHandle, msg, wParam, lParam);
    }
}

Window::~Window()
{
    if (WindowHandle)
    {
        UnregisterWindow(WindowHandle);
        DestroyWindow(WindowHandle);
    }
}

Window* Window::GetWindow(HWND hwnd)
{
    auto it = WindowRegistry.find(hwnd);
    if (it != WindowRegistry.end())
    {
        return it->second;
    }
    return nullptr;
}

bool Window::PeekMessages()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return true;
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return false;
}

LRESULT CALLBACK Window::StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* pThis = GetWindow(hwnd);
    if (pThis)
    {
        return pThis->WindowProcedure(msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}



