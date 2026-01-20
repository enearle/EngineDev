#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <iostream>
#include "Win32ErrorHandler.h"
#include "Win32Utils.h"

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
