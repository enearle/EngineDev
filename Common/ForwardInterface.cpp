#include "ForwardInterface.h"

#include "DirectX12/D3DCore.h"
#include "Vulkan/VulkanCore.h"


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

uint32_t ForwardInterface::GetVkQueueFamilyIndex()
{
    return VulkanCore::Instance().GetQueueFamilyIndex();
}

VkRenderPass ForwardInterface::GetNoesisCompatibilityRenderPass()
{
    return VulkanCore::Instance().GetNoesisCompatibilityRenderPass();
}

VkCommandBuffer ForwardInterface::GetCommandBuffer()
{
    return VulkanCore::Instance().GetCommandBuffer();
}

PFN_vkGetInstanceProcAddr ForwardInterface::GetInstanceProcAddress()
{
    return vkGetInstanceProcAddr;
}

ID3D12Device* ForwardInterface::GetD3D12Device()
{
    return D3DCore::Instance().GetDevice().Get();
}

ID3D12Fence* ForwardInterface::GetD3D12Fence()
{
    return D3DCore::Instance().GetFrameFence().Get();
}

DXGI_FORMAT ForwardInterface::GetD3D12RenderTargetFormat()
{
    return D3DCore::Instance().GetRenderTargetFormat();
}

DXGI_SAMPLE_DESC ForwardInterface::GetD3D12SampleDesc()
{
    return D3DCore::Instance().GetSampleDesc();
}

ID3D12GraphicsCommandList* ForwardInterface::GetCommandList()
{
    return D3DCore::Instance().GetCommandList().Get();
}
