#pragma once
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
};

extern RHI_API GraphicsSettings GRAPHICS_SETTINGS;