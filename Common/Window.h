#pragma once
#include <functional>
#include <map>
#include <mutex>
#include "Data/Event.h"
#include "Windows/WindowsHeaders.h"

enum WindowType
{
    Win32,
    GLFW
};

class Window
{
    HINSTANCE InstanceHandle;
    HWND WindowHandle;

    static std::map<HWND, Window*> WindowRegistry;

    int Width = 0, Height = 0;
    bool Resizing = false;

public:
    
    Window(LPCWSTR windowName, WindowType windowType, LONG xSize, LONG ySize);
    ~Window();

private:

    LRESULT WindowProcedure(UINT msg, WPARAM wParam, LPARAM lParam);
    
public:
    
    HINSTANCE GetInstance() const { return InstanceHandle; }
    HWND GetWindowHandle() const { return WindowHandle; }
    int GetWidth() const { return Width; }
    int GetHeight() const { return Height; }
    bool IsResizing() const { return Resizing; }
    bool PeekMessages();
    
    Event<int, int> OnResize;
    Event<> OnMinimize;
    Event<> OnMaximize;
    Event<> OnClose;

private:
    
    static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void RegisterWindow(HWND hwnd, Window* window) { WindowRegistry[hwnd] = window;}
    static void UnregisterWindow(HWND hwnd) { WindowRegistry.erase(hwnd); }
    static Window* GetWindow(HWND hwnd);
};
