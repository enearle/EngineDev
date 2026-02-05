#include <iostream>
#include "Win32ErrorHandler.h"
#include <ranges>
#include <format>


#include "Win32Utils.h"

namespace views = std::ranges::views;
namespace ranges = std::ranges;

using namespace Win32Utils;
void Win32ErrorHandler::Log(const char* msg)
{
    std::cout << msg << '\n';
}

void Win32ErrorHandler::ErrorMessage(const char* msg)
{
    ErrorMessageW(WidenString(msg).c_str());
}

void Win32ErrorHandler::ErrorMessage(const std::string& msg)
{
    ErrorMessage(msg.c_str());
}

void Win32ErrorHandler::ErrorMessageW(const wchar_t* msg)
{
    MessageBox(nullptr, msg, L"Error", MB_ICONERROR);
    Log(NarrowString(msg).c_str());
}

void Win32ErrorHandler::ErrorMessageW(const std::wstring& msg)
{
    ErrorMessageW(msg.c_str());
}

Win32ErrorHandler::HResultGrabber::HResultGrabber(unsigned int hr, std::source_location location) noexcept
    : HResult(hr), Location(location) {}

void Win32ErrorHandler::operator>>(HResultGrabber grabber, ErrorHandler)
{
    if (FAILED(grabber.HResult))
    {
        std::string error = GetErrorMessageFromHRESULT(grabber.HResult) |
            views::transform([](char c) {return c == '\n' ? ' ' : c;}) |
                views::filter([](char c) {return c != '\r';}) |
                    ranges::to<std::basic_string>();
        
        throw std::runtime_error(std::format(
            "DX12 Error: {}.\n  {}({})",
            error,
            grabber.Location.file_name(),
            grabber.Location.line()
            ));
    }
}

std::string Win32ErrorHandler::GetErrorMessageFromHRESULT(HRESULT hr)
{
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr
    );
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

std::wstring Win32ErrorHandler::GetErrorMessageWFromHRESULT(HRESULT hr)
{
    LPWSTR messageBuffer = nullptr;
    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0,
        nullptr
    );
    
    std::wstring message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

