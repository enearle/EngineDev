#pragma once

enum API
{
    DirectX12,
    Vulkan
};


inline struct GraphicsSettings
{
    API APIToUse = DirectX12;
    bool MSAA = false;
    bool HDR = false;
} GRAPHICS_SETTINGS;
