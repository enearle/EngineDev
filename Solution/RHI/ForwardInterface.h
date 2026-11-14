#pragma once
#include "GraphicsSettings.h"
#include "RHI_API_Macro.h"
#include "Windows/WindowsHeaders.h"
#include <d3d12.h>

// Native handles for code that records its own commands inside an overlay pass.
class RHI_API ForwardInterface
{
public:
    static API GetCurrentAPI() { return GRAPHICS_SETTINGS.APIToUse; }
    static uint32_t GetFramesInFlight();

    static VkSurfaceKHR GetVkSurface();
    static VkDevice GetVkDevice();
    static VkInstance GetVkInstance();
    static VkPhysicalDevice GetVkPhysicalDevice();
    static VkQueue GetVkGraphicsQueue();
    static uint32_t GetVkQueueFamilyIndex();
    static VkRenderPass GetVkOverlayRenderPass();
    static VkCommandBuffer GetVkCommandBuffer();
    static PFN_vkGetInstanceProcAddr GetVkInstanceProcAddress();

    static ID3D12Device* GetD3D12Device();
    static ID3D12CommandQueue* GetD3D12CommandQueue();
    static ID3D12Fence* GetD3D12Fence();
    static DXGI_FORMAT GetD3D12RenderTargetFormat();
    static DXGI_FORMAT GetD3D12OverlayDepthStencilFormat();
    static D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12RenderTargetDescriptor();
    static DXGI_SAMPLE_DESC GetD3D12SampleDesc();
    static ID3D12GraphicsCommandList* GetD3D12CommandList();
};
