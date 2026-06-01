#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "../ENGINE_API_Macro.h"

// Native-endian binary serialization over std::string-as-byte-buffer.
// Assumes a fixed target architecture (Windows x64 / ARM64, both little-endian).
// If you ever need cross-arch compatibility, swap the integer paths for
// explicit byte-order encoding.
class ENGINE_API AssetSerializer
{
public:
    template <typename T>
    static void Write(std::string& data, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "AssetSerializer::Write requires a trivially copyable type");
        data.append(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <typename T>
    static T Read(const std::string& data, long& offset)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "AssetSerializer::Read requires a trivially copyable type");
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    // Raw blob (e.g. a 64-byte matrix)
    static void WriteBytes(std::string& data, const void* src, size_t size)
    {
        data.append(static_cast<const char*>(src), size);
    }

    static void ReadBytes(const std::string& data, long& offset, void* dst, size_t size)
    {
        std::memcpy(dst, data.data() + offset, size);
        offset += static_cast<long>(size);
    }

    // Length-prefixed string (uint32_t length, then bytes)
    static void WriteString(std::string& data, const std::string& value)
    {
        Write<uint32_t>(data, static_cast<uint32_t>(value.size()));
        data.append(value);
    }

    static std::string ReadString(const std::string& data, long& offset)
    {
        uint32_t length = Read<uint32_t>(data, offset);
        std::string value = data.substr(offset, length);
        offset += static_cast<long>(length);
        return value;
    }
};
