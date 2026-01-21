#pragma once

enum API
{
    Direct3D12,
    Vulkan
};


inline struct GraphicsSettings
{
    API APIToUse = Vulkan;
    bool MSAA = false;
    bool HDR = false;
} GRAPHICS_SETTINGS;
