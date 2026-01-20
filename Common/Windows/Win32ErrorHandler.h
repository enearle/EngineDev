#pragma once
#include <string>
#include <windows.h>
#include <source_location>

namespace Win32ErrorHandler
{
    void Log(const char* msg);
    void ErrorMessage(const char* msg);
    void ErrorMessage(const std::string& msg);
    void ErrorMessageW(const wchar_t* msg);
    void ErrorMessageW(const std::wstring& msg);
    std::string GetErrorMessageFromHRESULT(HRESULT hr);
    std::wstring GetErrorMessageWFromHRESULT(HRESULT hr);

    inline struct ErrorHandler {} ERROR_HANDLER;
    struct HResultGrabber
    {
        HResultGrabber(unsigned int hr, std::source_location = std::source_location::current()) noexcept;
        unsigned int HResult;
        std::source_location Location;
    };
    void operator>>(HResultGrabber, ErrorHandler);
        
    
};
