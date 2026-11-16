#pragma once
#include <cstdlib>
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
    bool VSync = true;
    int Monitor = -1;               // Display the main window opens on, 0 being the primary; -1 lets Windows choose
    bool CanTakeFocus = true;       // False shows the main window without activating it

    void SetEditorMode(bool isEditor)
    {
        APIToUse = isEditor ? DirectX12 : Vulkan;
    }

    // -dx12 or -vulkan on the command line overrides the backend chosen in code; -novsync turns vsync off;
    // -monitor N and -no-focus place the window for unattended runs.
    void ApplyCommandLine(int argumentCount, char** arguments)
    {
        for (int i = 1; i < argumentCount; i++)
            if (std::strcmp(arguments[i], "-dx12") == 0)
                APIToUse = DirectX12;
            else if (std::strcmp(arguments[i], "-vulkan") == 0)
                APIToUse = Vulkan;
            else if (std::strcmp(arguments[i], "-novsync") == 0)
                VSync = false;
            else if (std::strcmp(arguments[i], "-monitor") == 0 && i + 1 < argumentCount)
                Monitor = std::atoi(arguments[++i]);
            else if (std::strcmp(arguments[i], "-no-focus") == 0)
                CanTakeFocus = false;
    }
};

extern RHI_API GraphicsSettings GRAPHICS_SETTINGS;
