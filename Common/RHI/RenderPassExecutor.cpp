#include "RenderPassExecutor.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include <DirectXMath.h>

#include "BufferAllocator.h"


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
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    D3DPipeline* d3dPipeline = static_cast<D3DPipeline*>(pipeline);
    
    // Set root signature and pipeline state
    cmdList->SetGraphicsRootSignature(d3dPipeline->GetRootSignature());
    cmdList->SetPipelineState(d3dPipeline->GetPipelineState());
    
    // Convert view handles to D3D12 format
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    if (!colorViews.empty() || depthView)
    {
        for (const auto& colorView : colorViews)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = reinterpret_cast<SIZE_T>(colorView);  // Reconstruct from the value
            rtvHandles.push_back(handle);
        }
    
        D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;
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
    }
    else
    {
        rtvHandles = d3dPipeline->GetOwnedRTVs();
        dsvHandle = d3dPipeline->GetOwnedDSV();
        
        if (!dsvHandle.ptr || rtvHandles.empty())
            throw std::runtime_error("No attachments provided.");
    }
    
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

void D3DRenderPassExecutor::DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& materialDescriptorSets)
{
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
    CurrentPipeline = static_cast<VulkanPipeline*>(pipeline);
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = {width, height};
    renderingInfo.layerCount = 1;
    
    VkImageView depthStencilView;
    std::vector<VkImageView> colourAttachmentViews;
    if (!colorViews.empty() || depthView)
    {
        for (const auto& colorView : colorViews)
            colourAttachmentViews.push_back(reinterpret_cast<VkImageView>(colorView));
        if (depthView)
            depthStencilView = reinterpret_cast<VkImageView>(depthView);
        
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
    }
    else
    {
        colourAttachmentViews = CurrentPipeline->GetOwnedImageViews();
        if (CurrentPipeline->GetOwnedDepthImageView())
            depthStencilView = CurrentPipeline->GetOwnedDepthImageView();
        else if (colourAttachmentViews.empty())
            throw std::runtime_error("No attachments provided.");
    }
    
    std::vector<VkAttachmentDescription> attachmentDescs = CurrentPipeline->GetAttachmentDescriptions();
    std::vector<VkRenderingAttachmentInfo> colourAttachments;
    for (size_t i = 0; i < colourAttachmentViews.size(); ++i)
    {
        VkRenderingAttachmentInfo attachment{};
        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = colourAttachmentViews[i];
        attachment.loadOp = attachmentDescs[i].loadOp;
        attachment.storeOp = attachmentDescs[i].storeOp;
        attachment.imageLayout = attachmentDescs[i].finalLayout;
        attachment.clearValue.color = {clearColors[i].x, clearColors[i].y, clearColors[i].z, clearColors[i].w};
        colourAttachments.push_back(attachment);
    }
    
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colourAttachmentViews.size());
    renderingInfo.pColorAttachments = colourAttachments.data();
    
    VkRenderingAttachmentInfo depthAttachment{};
    if (depthStencilView)
    {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthStencilView;
        depthAttachment.loadOp = attachmentDescs[colourAttachmentViews.size()].loadOp;
        depthAttachment.storeOp = attachmentDescs[colourAttachmentViews.size()].storeOp;
        depthAttachment.imageLayout = attachmentDescs[colourAttachmentViews.size()].finalLayout;
        depthAttachment.clearValue.depthStencil = {clearDepth, 0};
        renderingInfo.pDepthAttachment = &depthAttachment;
    }
    
    vkCmdBeginRendering(cmdBuffer, &renderingInfo);
    
    // Bind pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline->GetVulkanPipeline());
    
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
    vkCmdEndRendering(cmdBuffer);
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

void VulkanRenderPassExecutor::DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& materialDescriptorSets)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    VulkanBufferAllocator* alloc = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());
    
        // Get Vulkan function pointers
    PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT_FnPtr = 
        VulkanCore::GetInstance().GetVkCmdBindDescriptorBuffersEXT();
    PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT_FnPtr = 
        VulkanCore::GetInstance().GetVkCmdSetDescriptorBufferOffsetsEXT();
    
    // Draw all meshes in this node
    for (size_t i = 0; i < node.GetMeshCount(); i++)
    {
        const Mesh* mesh = node.GetMesh(i);
        uint32_t materialIndex = mesh->GetLocalMaterialIndex();
        
        // Bind descriptor set for this material
        if (materialIndex < materialDescriptorSets.size())
        {
            VkDeviceAddress address = alloc->GetDescriptorBufferAddress();
            VkDeviceSize offset = materialDescriptorSets[materialIndex] - address;
            uint32_t bufferIndex = 0;
            
            VkDescriptorBufferBindingInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
            info.address = address;
            info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                        VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
            info.pNext = nullptr;
            
            vkCmdBindDescriptorBuffersEXT_FnPtr(cmdBuffer, 1, &info);
            vkCmdSetDescriptorBufferOffsetsEXT_FnPtr(
                cmdBuffer, 
                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                CurrentPipeline->GetPipelineLayout(), 
                0, 1, &bufferIndex, &offset);
        }
        
        // Bind vertex and index buffers
        // NOTE: You'll need to add buffer handles to your Mesh class
        // VkBuffer vertexBuffer = mesh->GetVertexBuffer();
        // VkBuffer indexBuffer = mesh->GetIndexBuffer();
        // VkDeviceSize offsets[] = {0};
        // vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, offsets);
        // vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        
        // Draw indexed
        // vkCmdDrawIndexed(cmdBuffer, mesh->GetIndexCount(), 1, 0, 0, 0);
        
        // For now, if you don't have buffers set up yet:
        vkCmdDraw(cmdBuffer, mesh->GetVertexCount(), 1, 0, 0);
    }
    
}

VkCommandBuffer VulkanRenderPassExecutor::GetCommandBuffer()
{
    return VulkanCore::GetInstance().GetCommandBuffer();
}

