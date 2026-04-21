#pragma once
#include "GraphicsSettings.h"
#include "RHI_API_Macro.h"
#include "Windows/WindowsHeaders.h"
#include <d3d12.h>

class RHI_API ForwardInterface
{
public:
    static API GetCurrentAPI() { return GRAPHICS_SETTINGS.APIToUse; }
    
    static VkSurfaceKHR GetVkSurface();
    static VkDevice GetVkDevice();
    static VkInstance GetVkInstance();
    static VkPhysicalDevice GetVkPhysicalDevice();
    static uint32_t GetVkQueueFamilyIndex();
    static VkRenderPass GetNoesisCompatibilityRenderPass();
    static VkCommandBuffer GetCommandBuffer();
    static PFN_vkGetInstanceProcAddr GetInstanceProcAddress();
    static uint32_t GetVulkanFramesInFlight();
    
    static ID3D12Device* GetD3D12Device();
    static ID3D12Fence* GetD3D12Fence();
    static DXGI_FORMAT GetD3D12RenderTargetFormat();
    static DXGI_SAMPLE_DESC GetD3D12SampleDesc();
    static ID3D12GraphicsCommandList* GetCommandList();
    static uint32_t GetD3D12FramesInFlight();
    
};
