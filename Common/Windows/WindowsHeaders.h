#pragma once

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
    #define _WIN64 1
#elif defined(_WIN32) || defined(__i386__)
    #define _WIN32 1
#else
    #error "Unknown or unsupported architecture"
#endif


#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <wrl.h>
#include <winuser.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#pragma comment(lib, "vulkan-1.lib")
