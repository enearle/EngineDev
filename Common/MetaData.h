#pragma once
#include <cstdint>

class MetaData
{
private:

    static const char* APP_NAME;
    static const uint32_t APP_MAJOR;
    static const uint32_t APP_MINOR;
    static const uint32_t APP_PATCH;
    
    static const char* ENGINE_NAME;
    static const uint32_t ENGINE_MAJOR;
    static const uint32_t ENGINE_MINOR;
    static const uint32_t ENGINE_PATCH;

public:
    static const char* GetAppName() { return APP_NAME; }
    static const char* GetEngineName() { return ENGINE_NAME; }
    static uint32_t GetAppMajor() { return APP_MAJOR; }
    static uint32_t GetAppMinor() { return APP_MINOR; }
    static uint32_t GetAppPatch() { return APP_PATCH; }
    static uint32_t GetEngineMajor() { return ENGINE_MAJOR; }
    static uint32_t GetEngineMinor() { return ENGINE_MINOR; }
    static uint32_t GetEnginePatch() { return ENGINE_PATCH; }
    
};


