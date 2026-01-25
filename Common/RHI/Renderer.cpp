#include "Renderer.h"

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"

void Renderer::StartRender(Window* window)
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case Direct3D12:
        D3DCore::GetInstance().InitDirect3D(window);
        break;
    case Vulkan:
        VulkanCore::GetInstance().InitVulkan(window);
        break;
    default:
        throw std::runtime_error("Invalid graphics API selected.");
    }
}

void Renderer::EndRender()
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case Direct3D12:
        D3DCore::GetInstance().Reset();
        break;
    case Vulkan:
        VulkanCore::GetInstance().Cleanup();
        break;
    default:
        throw std::runtime_error("Invalid graphics API selected.");
    }
}
