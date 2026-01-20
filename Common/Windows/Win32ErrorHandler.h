#pragma once
#include <string>
#include <windows.h>

namespace Win32ErrorHandler
{
    void Log(const char* msg);
    void ErrorMessage(const char* msg);
    void ErrorMessage(const std::string& msg);
    void ErrorMessageW(const wchar_t* msg);
    void ErrorMessageW(const std::wstring& msg);
};
