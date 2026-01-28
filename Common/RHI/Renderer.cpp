#include "Renderer.h"

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"

void Renderer::StartRender(Window* window, CoreInitData data)
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case DirectX12:
        D3DCore::GetInstance().InitDirect3D(window, data);
        break;
    case Vulkan:
        VulkanCore::GetInstance().InitVulkan(window, data);
        break;
    }
}

void Renderer::EndRender()
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case DirectX12:
        D3DCore::GetInstance().Reset();
        break;
    case Vulkan:
        VulkanCore::GetInstance().Cleanup();
        break;
    }
}

void Renderer::BeginFrame()
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case DirectX12:
        D3DCore::GetInstance().BeginFrame();
        break;
    case Vulkan:
        VulkanCore::GetInstance().BeginFrame();
        break;
    }
}

void Renderer::EndFrame()
{
    switch (GRAPHICS_SETTINGS.APIToUse)
    {
    case DirectX12:
        D3DCore::GetInstance().EndFrame();
        break;
    case Vulkan:
        VulkanCore::GetInstance().EndFrame();
        break;
    }
}

void Renderer::GetSwapChainRenderTargets(std::vector<void*>& outColorViews)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        throw std::runtime_error("No vulkan support for swapchain RTs!");
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
    {
       D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = D3DCore::GetInstance().GetRenderTargetDescriptor();
       outColorViews.clear();
       outColorViews.push_back(reinterpret_cast<void*>(rtvHandle.ptr)); 
    }
    
}
