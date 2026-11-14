#include "ForwardInterface.h"

#include "DirectX12/D3DCore.h"
#include "Vulkan/VulkanCore.h"

uint32_t ForwardInterface::GetFramesInFlight()
{
    return GRAPHICS_SETTINGS.APIToUse == Vulkan
        ? VulkanCore::Instance().GetFramesInFlight()
        : D3DCore::Instance().GetFramesInFlight();
}

VkSurfaceKHR ForwardInterface::GetVkSurface()
{
    return VulkanCore::Instance().GetSurface();
}

VkDevice ForwardInterface::GetVkDevice()
{
    return VulkanCore::Instance().GetDevice();
}

VkInstance ForwardInterface::GetVkInstance()
{
    return VulkanCore::Instance().GetInstance();
}

VkPhysicalDevice ForwardInterface::GetVkPhysicalDevice()
{
    return VulkanCore::Instance().GetPhysicalDevice();
}

VkQueue ForwardInterface::GetVkGraphicsQueue()
{
    return VulkanCore::Instance().GetGraphicsQueue();
}

uint32_t ForwardInterface::GetVkQueueFamilyIndex()
{
    return VulkanCore::Instance().GetQueueFamilyIndex();
}

VkRenderPass ForwardInterface::GetVkOverlayRenderPass()
{
    return VulkanCore::Instance().GetOverlayRenderPass();
}

VkCommandBuffer ForwardInterface::GetVkCommandBuffer()
{
    return VulkanCore::Instance().GetCommandBuffer();
}

PFN_vkGetInstanceProcAddr ForwardInterface::GetVkInstanceProcAddress()
{
    return vkGetInstanceProcAddr;
}

ID3D12Device* ForwardInterface::GetD3D12Device()
{
    return D3DCore::Instance().GetDevice().Get();
}

ID3D12CommandQueue* ForwardInterface::GetD3D12CommandQueue()
{
    return D3DCore::Instance().GetCommandQueue().Get();
}

ID3D12Fence* ForwardInterface::GetD3D12Fence()
{
    return D3DCore::Instance().GetFrameFence().Get();
}

DXGI_FORMAT ForwardInterface::GetD3D12RenderTargetFormat()
{
    return D3DCore::Instance().GetRenderTargetFormat();
}

DXGI_FORMAT ForwardInterface::GetD3D12OverlayDepthStencilFormat()
{
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
}

D3D12_CPU_DESCRIPTOR_HANDLE ForwardInterface::GetD3D12RenderTargetDescriptor()
{
    return D3DCore::Instance().GetRenderTargetDescriptor();
}

DXGI_SAMPLE_DESC ForwardInterface::GetD3D12SampleDesc()
{
    return D3DCore::Instance().GetSampleDesc();
}

ID3D12GraphicsCommandList* ForwardInterface::GetD3D12CommandList()
{
    return D3DCore::Instance().GetCommandList().Get();
}
