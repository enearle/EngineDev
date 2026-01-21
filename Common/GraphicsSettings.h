#pragma once

enum API
{
    Direct3D12,
    Vulkan
};


inline struct GraphicsSettings
{
    API APIToUse = Direct3D12;
    bool MSAA = false;
    bool HDR = false;
} GRAPHICS_SETTINGS;
