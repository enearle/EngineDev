#pragma once
#include <string>
#include <windows.h>

namespace Win32Utils
{
    inline std::string NarrowString(const std::wstring& wideString)
    {
        if (wideString.empty()) return std::string();
        int sizeNeeded = WideCharToMultiByte(CP_ACP, 0, &wideString[0], (int)wideString.size(),
            nullptr, 0, nullptr, nullptr);
        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_ACP, 0, &wideString[0], (int)wideString.size(), &result[0],
            sizeNeeded, nullptr, nullptr);
        return result;
    }

    inline std::wstring WidenString(const std::string& narrowString)
    {
        if (narrowString.empty()) return std::wstring();
        int sizeNeeded = MultiByteToWideChar(CP_ACP, 0, &narrowString[0], (int)narrowString.size(),
            nullptr, 0);
        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_ACP, 0, &narrowString[0], (int)narrowString.size(), &result[0],
            sizeNeeded);
    }

};
