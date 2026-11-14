#pragma once
#include <cstring>
#include "RHI_API_Macro.h"

enum API
{
    DirectX12,
    Vulkan
};

inline struct GraphicsSettings
{
    API APIToUse = Vulkan;
    bool MSAA = false;
    bool HDR = false;

    void SetEditorMode(bool isEditor)
    {
        APIToUse = isEditor ? DirectX12 : Vulkan;
    }

    // -dx12 or -vulkan on the command line overrides the backend chosen in code.
    void ApplyCommandLine(int argumentCount, char** arguments)
    {
        for (int i = 1; i < argumentCount; i++)
            if (std::strcmp(arguments[i], "-dx12") == 0)
                APIToUse = DirectX12;
            else if (std::strcmp(arguments[i], "-vulkan") == 0)
                APIToUse = Vulkan;
    }
};

extern RHI_API GraphicsSettings GRAPHICS_SETTINGS;
