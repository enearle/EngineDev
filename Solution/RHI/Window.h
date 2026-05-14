#pragma once
#include <functional>
#include <map>
#include "Data/Event.h"
#include "Windows/WindowsHeaders.h"
#include "RHI_API_Macro.h"

enum WindowType
{
    Win32,
    GLFW
};

class RHI_API Window
{
    HINSTANCE InstanceHandle;
    HWND WindowHandle;
    bool OwnsWindow = true;  // false when wrapping an external HWND

    static std::map<HWND, Window*> WindowRegistry;

    uint32_t Width = 0, Height = 0;
    bool Resizing = false;

public:
    
    Window(LPCWSTR windowName, WindowType windowType, LONG xSize, LONG ySize);
    Window(HWND parentHwnd, LONG x, LONG y, LONG width, LONG height);
    explicit Window(HWND existingHwnd);  // Wraps an HWND owned by another process — no window creation
    ~Window();

private:

    LRESULT WindowProcedure(UINT msg, WPARAM wParam, LPARAM lParam);
    
public:
    
    HINSTANCE GetInstance() const { return InstanceHandle; }
    HWND GetWindowHandle() const { return WindowHandle; }
    uint32_t GetWidth() const { return Width; }
    uint32_t GetHeight() const { return Height; }
    bool IsResizing() const { return Resizing; }
    bool PeekMessages();
    void PollResize();  // For embedded mode: checks HWND client size and fires OnResize if changed
    
    Event<uint32_t, uint32_t> OnResize;
    Event<> OnMinimize;
    Event<> OnMaximize;
    Event<> OnClose;

private:
    
    static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void RegisterWindow(HWND hwnd, Window* window) { WindowRegistry[hwnd] = window;}
    static void UnregisterWindow(HWND hwnd) { WindowRegistry.erase(hwnd); }
    static Window* GetWindow(HWND hwnd);
};
