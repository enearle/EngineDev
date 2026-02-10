#include "RenderPassExecutor.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include <DirectXMath.h>


RenderPassExecutor* RenderPassExecutor::Create()
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
    {
        return new VulkanRenderPassExecutor();
    }
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
    {
        return new D3DRenderPassExecutor();
    }
    else
    {
        throw std::runtime_error("Invalid graphics API selected.");
    }
}

//================================================//
// DirectX 12                                     //
//================================================//

D3DRenderPassExecutor::D3DRenderPassExecutor() = default;
D3DRenderPassExecutor::~D3DRenderPassExecutor() = default;

void D3DRenderPassExecutor::Begin(Pipeline* pipeline,
                                   const std::vector<void*>& colorViews,
                                   void* depthView,
                                   uint32_t width, uint32_t height,
                                   const std::vector<DirectX::XMFLOAT4>& clearColors,
                                   float clearDepth)
{
    // Validate inputs
    if (colorViews.size() != clearColors.size())
    {
        throw std::runtime_error(
            "Mismatch: colorViews.size() (" + std::to_string(colorViews.size()) + 
            ") must equal clearColors.size() (" + std::to_string(clearColors.size()) + ")"
        );
    }
    
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    D3DPipeline* d3dPipeline = static_cast<D3DPipeline*>(pipeline);
    
    // Set root signature and pipeline state
    cmdList->SetGraphicsRootSignature(d3dPipeline->GetRootSignature());
    cmdList->SetPipelineState(d3dPipeline->GetPipelineState());
    
    // Convert view handles to D3D12 format
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    for (const auto& colorView : colorViews)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = reinterpret_cast<SIZE_T>(colorView);  // Reconstruct from the value
        rtvHandles.push_back(handle);
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    if (depthView)
    {
        dsvHandle = *reinterpret_cast<D3D12_CPU_DESCRIPTOR_HANDLE*>(depthView);
        pDsvHandle = &dsvHandle;
    }
    
    cmdList->OMSetRenderTargets(
        static_cast<UINT>(rtvHandles.size()),
        rtvHandles.data(),
        FALSE,
        pDsvHandle
    );
    
    // Clear render targets
    for (size_t i = 0; i < colorViews.size(); ++i)
    {
        float clearColor[] = {
            clearColors[i].x,
            clearColors[i].y,
            clearColors[i].z,
            clearColors[i].w
        };
        cmdList->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
    }
    
    if (depthView)
    {
        cmdList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearDepth,
            0,  // stencil clear value
            0,
            nullptr
        );
    }

    cmdList->IASetPrimitiveTopology( dynamic_cast<D3DPipeline*>(pipeline)->GetTopology());
    
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<FLOAT>(width);
    viewport.Height = static_cast<FLOAT>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);
    
    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(width);
    scissor.bottom = static_cast<LONG>(height);
    cmdList->RSSetScissorRects(1, &scissor);
}

void D3DRenderPassExecutor::End()
{
    // Empty atm

}

void D3DRenderPassExecutor::BindPipeline(Pipeline* pipeline)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    D3DPipeline* d3dPipeline = static_cast<D3DPipeline*>(pipeline);
    
    cmdList->SetGraphicsRootSignature(d3dPipeline->GetRootSignature());
    cmdList->SetPipelineState(d3dPipeline->GetPipelineState());
}

void D3DRenderPassExecutor::IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    
    // Global memory barrier (UAV barrier)
    D3D12_RESOURCE_BARRIER d3dBarrier{};
    d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    d3dBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    
    cmdList->ResourceBarrier(1, &d3dBarrier);
}

void D3DRenderPassExecutor::IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    ID3D12Resource* resource = reinterpret_cast<ID3D12Resource*>(barrier.ImageResource);
    
    // Convert layout enums to D3D12 resource states
    D3D12_RESOURCE_STATES stateBefore = ConvertLayoutToResourceState(barrier.OldLayout);
    D3D12_RESOURCE_STATES stateAfter = ConvertLayoutToResourceState(barrier.NewLayout);
    
    D3D12_RESOURCE_BARRIER d3dBarrier{};
    d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    d3dBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    d3dBarrier.Transition.pResource = resource;
    d3dBarrier.Transition.StateBefore = stateBefore;
    d3dBarrier.Transition.StateAfter = stateAfter;
    d3dBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    cmdList->ResourceBarrier(1, &d3dBarrier);
}

ID3D12GraphicsCommandList* D3DRenderPassExecutor::GetCommandList()
{
    return D3DCore::GetInstance().GetCommandList().Get();
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanRenderPassExecutor::VulkanRenderPassExecutor()
{
    uint32_t swapchainImageCount = VulkanCore::GetInstance().GetSwapchainImageCount();
    Framebuffers.resize(swapchainImageCount, VK_NULL_HANDLE);
}

VulkanRenderPassExecutor::~VulkanRenderPassExecutor()
{
    InvalidateFramebuffers();
}

void VulkanRenderPassExecutor::Begin(Pipeline* pipeline,
                                     const std::vector<void*>& colorViews,
                                     void* depthView,
                                     uint32_t width, uint32_t height,
                                     const std::vector<DirectX::XMFLOAT4>& clearColors,
                                     float clearDepth)
{
    // Set framebuffer
    VulkanPipeline* vulkanPipeline = static_cast<VulkanPipeline*>(pipeline);
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    
    std::vector<VkImageView> attachmentViews;
    for (const auto& colorView : colorViews)
        attachmentViews.push_back(reinterpret_cast<VkImageView>(colorView));
    if (depthView)
        attachmentViews.push_back(reinterpret_cast<VkImageView>(depthView));
    
    uint32_t swapchainImageIndex = VulkanCore::GetInstance().GetCurrentSwapchainImageIndex();
    VkFramebuffer& framebuffer = Framebuffers[swapchainImageIndex];
    
    if (framebuffer == VK_NULL_HANDLE)
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vulkanPipeline->GetRenderPass();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.pAttachments = attachmentViews.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(VulkanCore::GetInstance().GetDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
    
    // Prepare clear values
    std::vector<VkClearValue> clearValues;
    for (size_t i = 0; i < clearColors.size(); ++i)
    {
        VkClearValue clearValue{};
        clearValue.color = {clearColors[i].x, clearColors[i].y, clearColors[i].z, clearColors[i].w};
        clearValues.push_back(clearValue);
    }
    if (depthView)
    {
        VkClearValue depthClear{};
        depthClear.depthStencil = {clearDepth, 0};
        clearValues.push_back(depthClear);
    }
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = vulkanPipeline->GetRenderPass();
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = {width, height};
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetVulkanPipeline());
    
    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(height);
    viewport.width = static_cast<float>(width);
    viewport.height = -static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    
    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}

void VulkanRenderPassExecutor::End()
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    vkCmdEndRenderPass(cmdBuffer);
}

void VulkanRenderPassExecutor::BindPipeline(Pipeline* pipeline)
{
    VulkanPipeline* vulkanPipeline = static_cast<VulkanPipeline*>(pipeline);
    vkCmdBindPipeline(GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetVulkanPipeline());
}

void VulkanRenderPassExecutor::IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier)
{
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = barrier.SrcAccessMask;
    memBarrier.dstAccessMask = barrier.DstAccessMask;
    
    vkCmdPipelineBarrier(
        GetCommandBuffer(),
        static_cast<VkPipelineStageFlags>(barrier.SrcStage),
        static_cast<VkPipelineStageFlags>(barrier.DstStage),
        0,
        1, &memBarrier,
        0, nullptr,
        0, nullptr
    );
}

void VulkanRenderPassExecutor::IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    
    VkImageMemoryBarrier vkBarrier{};
    vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkBarrier.pNext = nullptr;
    vkBarrier.oldLayout = VulkanImageLayout(barrier.OldLayout);  // Use conversion function
    vkBarrier.newLayout = VulkanImageLayout(barrier.NewLayout);  // Use conversion function
    vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier.image = reinterpret_cast<VkImage>(barrier.ImageResource);
    vkBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBarrier.subresourceRange.baseMipLevel = barrier.BaseMipLevel;
    vkBarrier.subresourceRange.levelCount = barrier.MipLevelCount;
    vkBarrier.subresourceRange.baseArrayLayer = barrier.BaseArrayLayer;
    vkBarrier.subresourceRange.layerCount = barrier.ArrayLayerCount;
    vkBarrier.srcAccessMask = barrier.SrcAccessMask;
    vkBarrier.dstAccessMask = barrier.DstAccessMask;
    
    VkPipelineStageFlags srcStage = ConvertPipelineStage(barrier.SrcStage);
    VkPipelineStageFlags dstStage = ConvertPipelineStage(barrier.DstStage);
    
    // Ensure we never pass 0 for srcStageMask
    if (srcStage == 0)
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (dstStage == 0)
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    
    vkCmdPipelineBarrier(
        cmdBuffer,
        srcStage,
        dstStage,
        0,  // No dependency flags
        0, nullptr,  // No memory barriers
        0, nullptr,  // No buffer memory barriers
        1, &vkBarrier
    );
}

VkCommandBuffer VulkanRenderPassExecutor::GetCommandBuffer()
{
    return VulkanCore::GetInstance().GetCommandBuffer();
}

void VulkanRenderPassExecutor::InvalidateFramebuffers()
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    for (VkFramebuffer framebuffer : Framebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    
    uint32_t swapchainImageCount = VulkanCore::GetInstance().GetSwapchainImageCount();
    Framebuffers.clear();
    Framebuffers.resize(swapchainImageCount, VK_NULL_HANDLE);
}
