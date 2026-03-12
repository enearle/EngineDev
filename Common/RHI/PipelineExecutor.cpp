#include "PipelineExecutor.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include <DirectXMath.h>
#include "../DirectX12/D3D12Structs.h"
#include "BufferAllocator.h"
#include "RHIConstants.h"

using namespace D3D12Structs;

PipelineExecutor* PipelineExecutor::Create(Window* window, CoreInitData data)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
    {
        return new VulkanPipelineExecutor(window, data);
    }
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
    {
        return new D3DPipelineExecutor(window, data);
    }
    else
    {
        throw std::runtime_error("Invalid graphics API selected.");
    }
}

//================================================//
// DirectX 12                                     //
//================================================//

D3DPipelineExecutor::D3DPipelineExecutor(Window* window, CoreInitData data)
{
    StartRender(window, data);
}

D3DPipelineExecutor::~D3DPipelineExecutor()
{
    EndRender();
}

void D3DPipelineExecutor::StartRender(Window* window, CoreInitData data)
{
    D3DCore::GetInstance().InitDirect3D(window, data);
}

void D3DPipelineExecutor::EndRender()
{
    D3DCore::GetInstance().Reset();
}

void D3DPipelineExecutor::BeginFrame()
{
    D3DCore::GetInstance().BeginFrame();
}

void D3DPipelineExecutor::EndFrame()
{
    D3DCore::GetInstance().EndFrame();
}

void D3DPipelineExecutor::GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = D3DCore::GetInstance().GetRenderTargetDescriptor();
    ID3D12Resource* backBuffer = D3DCore::GetInstance().GetCurrentBackBuffer();
    outBackBufferView = reinterpret_cast<void*>(rtvHandle.ptr);
    outBackBuffer = backBuffer;
}

void D3DPipelineExecutor::Wait()
{
    D3DCore::GetInstance().WaitForGPU();
}

void D3DPipelineExecutor::BeginPipeline(Pipeline* pipeline,
                                        const std::vector<void*>& colorViews,
                                        void* depthView,
                                        uint32_t width, uint32_t height)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    CurrentPipeline = static_cast<D3DPipeline*>(pipeline);
    
    // Set root signature and pipeline state
    cmdList->SetGraphicsRootSignature(CurrentPipeline->GetRootSignature());
    cmdList->SetPipelineState(CurrentPipeline->GetPipelineState());
    
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    DirectX12BufferAllocator* dxAlloc = static_cast<DirectX12BufferAllocator*>(bufferAlloc);
    ID3D12DescriptorHeap* heaps[] = { dxAlloc->GetShaderResourceHeap().Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    
    // Convert view handles to D3D12 format
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;

    if (!colorViews.empty() || depthView)
    {
        for (const auto& colorView : colorViews)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = reinterpret_cast<SIZE_T>(colorView);
            rtvHandles.push_back(handle);
        }

        if (depthView)
        {
            dsvHandle = *reinterpret_cast<D3D12_CPU_DESCRIPTOR_HANDLE*>(depthView);
            pDsvHandle = &dsvHandle;
        }
    }
    else
    {
        rtvHandles = CurrentPipeline->GetOwnedRTVs();
        dsvHandle = CurrentPipeline->GetOwnedDSV();
    
        if (dsvHandle.ptr != 0)
        {
            pDsvHandle = &dsvHandle;
        }
    
        if (rtvHandles.empty() && !pDsvHandle)
            throw std::runtime_error("No attachments provided.");
    }

    // Set render targets (for both external and owned attachments)
    cmdList->OMSetRenderTargets(
        static_cast<UINT>(rtvHandles.size()),
        rtvHandles.empty() ? nullptr : rtvHandles.data(),
        FALSE,
        pDsvHandle
    );

    // Clear render targets
    for (size_t i = 0; i < rtvHandles.size() && i < CurrentPipeline->GetClearColors().size(); ++i)
    {
        float clearColor[] = {
            CurrentPipeline->GetClearColors()[i].x,
            CurrentPipeline->GetClearColors()[i].y,
            CurrentPipeline->GetClearColors()[i].z,
            CurrentPipeline->GetClearColors()[i].w
        };
        cmdList->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
    }

    if (pDsvHandle)
    {
        cmdList->ClearDepthStencilView(
            *pDsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            CurrentPipeline->GetDepthClearValue(),
            0,
            0,
            nullptr
        );
    }

    cmdList->IASetPrimitiveTopology(dynamic_cast<D3DPipeline*>(pipeline)->GetTopology());
    
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

void D3DPipelineExecutor::EndPipeline()
{
    // Empty atm

}

void D3DPipelineExecutor::IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    
    // Global memory barrier (UAV barrier)
    D3D12_RESOURCE_BARRIER d3dBarrier{};
    d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    d3dBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    
    cmdList->ResourceBarrier(1, &d3dBarrier);
}

void D3DPipelineExecutor::IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier)
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

void D3DPipelineExecutor::DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, 
    uint32_t indexCount, void* pushConstant, size_t pushConstantSize)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    if (pushConstant && pushConstantSize > 0)
    {
        uint32_t num32BitValues = static_cast<uint32_t>(pushConstantSize / 4);
        cmdList->SetGraphicsRoot32BitConstants(0, num32BitValues, pushConstant, 0);
    }
    
    BufferAllocation vertAlloc = bufferAlloc->GetBufferAllocation(vertBufferID);
    BufferAllocation indexAlloc = bufferAlloc->GetBufferAllocation(indexBufferID);
    
    // Get GPU virtual addresses from the buffer data
    DX12BufferData* vertBufferData = DXBuffer(vertAlloc);
    DX12BufferData* indexBufferData = DXBuffer(indexAlloc);
    
    // Reconstructing views per draw call
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vertBufferData->GPUAddress;
    vbv.SizeInBytes = static_cast<UINT>(vertAlloc.Size);
    vbv.StrideInBytes = sizeof(Vertex);
    
    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = indexBufferData->GPUAddress;
    ibv.SizeInBytes = static_cast<UINT>(indexAlloc.Size);
    ibv.Format = DXGI_FORMAT_R32_UINT;

    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetIndexBuffer(&ibv);
    cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void D3DPipelineExecutor::DrawFSQuad()
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    cmdList->DrawInstanced(6, 1, 0, 0);
}

void D3DPipelineExecutor::BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    D3DPipeline* d3dPipeline = static_cast<D3DPipeline*>(CurrentPipeline);
    uint32_t rootParamIndex = numPipelineSets + CurrentPipeline->GetPushConstantCount();
    
    if (descriptorSets)
    {
        for (size_t i = 0; i < descriptorSets->size(); i++)
        {
            uint64_t descriptorSetID = descriptorSets->at(i);
            DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(descriptorSetID);

            D3D12_GPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = allocation.DescriptorAddress;
            
            cmdList->SetGraphicsRootDescriptorTable(rootParamIndex++, handle);
        }
    }
}

void D3DPipelineExecutor::BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    uint32_t rootParamIndex = CurrentPipeline->GetPushConstantCount();
    
    if (descriptorSets)
    {
        for (size_t i = 0; i < descriptorSets->size(); i++)
        {
            uint64_t descriptorSetID = descriptorSets->at(i);
            DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(descriptorSetID);
            
            D3D12_GPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = allocation.DescriptorAddress;
            
            cmdList->SetGraphicsRootDescriptorTable(rootParamIndex++, handle);
        }
    }
    
    for (uint64_t descriptorSetID : CurrentPipeline->GetInputDescriptorSetIDs())
    {
        DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(descriptorSetID);
        
        D3D12_GPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = allocation.DescriptorAddress;
        cmdList->SetGraphicsRootDescriptorTable(rootParamIndex++, handle);
    }
}

ID3D12GraphicsCommandList* D3DPipelineExecutor::GetCommandList()
{
    return D3DCore::GetInstance().GetCommandList().Get();
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanPipelineExecutor::VulkanPipelineExecutor(Window* window, CoreInitData data)
{
    StartRender(window, data);
}

VulkanPipelineExecutor::~VulkanPipelineExecutor()
{
    EndRender();
}

void VulkanPipelineExecutor::StartRender(Window* window, CoreInitData data)
{
    VulkanCore::GetInstance().InitVulkan(window, data);
}

void VulkanPipelineExecutor::EndRender()
{
    VulkanCore::GetInstance().Cleanup();
}

void VulkanPipelineExecutor::BeginFrame()
{
    VulkanCore::GetInstance().BeginFrame();
}

void VulkanPipelineExecutor::EndFrame()
{
    VulkanCore::GetInstance().EndFrame();
}

void VulkanPipelineExecutor::GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer)
{
    VkImageView swapchainImageView = VulkanCore::GetInstance().GetCurrentSwapchainImageView();
    VkImage vkImage = VulkanCore::GetInstance().GetCurrentSwapchainImage();
    outBackBufferView = swapchainImageView;
    outBackBuffer = reinterpret_cast<void*>(vkImage);
}

void VulkanPipelineExecutor::Wait()
{
    VulkanCore::GetInstance().WaitForGPU();
}

void VulkanPipelineExecutor::BeginPipeline(Pipeline* pipeline,
                                           const std::vector<void*>& colorViews,
                                           void* depthView,
                                           uint32_t width, uint32_t height)
{
    CurrentPipeline = static_cast<VulkanPipeline*>(pipeline);
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = {width, height};
    renderingInfo.layerCount = 1;
    
    VkImageView depthStencilView = VK_NULL_HANDLE;
    std::vector<VkImageView> colourAttachmentViews;
    if (!colorViews.empty())
        for (const auto& colorView : colorViews)
            colourAttachmentViews.push_back(reinterpret_cast<VkImageView>(colorView));
    else
        colourAttachmentViews = CurrentPipeline->GetOwnedImageViews();
    
    if (depthView)
        depthStencilView = reinterpret_cast<VkImageView>(depthView);
    else 
        depthStencilView = CurrentPipeline->GetOwnedDepthImageView();
    
    if (colourAttachmentViews.empty() && depthStencilView == VK_NULL_HANDLE)
        throw std::runtime_error("No attachments provided.");
    
    if (CurrentPipeline->GetClearColors().size() != colourAttachmentViews.size())
        throw std::runtime_error("Number of clear colors does not match number of attachments.");
    
    std::vector<VkAttachmentDescription> attachmentDescs = CurrentPipeline->GetAttachmentDescriptions();
    VkAttachmentDescription depthStencilDesc = {};  // Initialize to zero
    if (depthStencilView != VK_NULL_HANDLE) 
        depthStencilDesc = CurrentPipeline->GetDepthAttachmentDescription();
    std::vector<VkRenderingAttachmentInfo> colourAttachments;
    size_t numColourAttachments = colourAttachmentViews.size();
    
    for (size_t i = 0; i < numColourAttachments; ++i)
    {
        VkRenderingAttachmentInfo attachment{};
        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = colourAttachmentViews[i];
        attachment.loadOp = attachmentDescs[i].loadOp;
        attachment.storeOp = attachmentDescs[i].storeOp;
        attachment.imageLayout = attachmentDescs[i].finalLayout;
        attachment.clearValue.color = {CurrentPipeline->GetClearColors()[i].x, CurrentPipeline->GetClearColors()[i].y, CurrentPipeline->GetClearColors()[i].z, CurrentPipeline->GetClearColors()[i].w};
        colourAttachments.push_back(attachment);
    }
    
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colourAttachments.size());
    renderingInfo.pColorAttachments = colourAttachments.data();
    
    VkRenderingAttachmentInfo depthAttachment{};
    if (depthStencilView != VK_NULL_HANDLE)
    {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthStencilView;
        depthAttachment.loadOp = depthStencilDesc.loadOp;
        depthAttachment.storeOp = depthStencilDesc.storeOp;
        depthAttachment.imageLayout = depthStencilDesc.finalLayout;
        depthAttachment.clearValue.depthStencil = {CurrentPipeline->GetDepthClearValue(), 0};
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

void VulkanPipelineExecutor::EndPipeline()
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    vkCmdEndRendering(cmdBuffer);
}

void VulkanPipelineExecutor::IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier)
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

void VulkanPipelineExecutor::IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier)
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
    vkBarrier.subresourceRange.aspectMask = barrier.IsDepthImage ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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

void VulkanPipelineExecutor::DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, 
    uint32_t indexCount, void* pushConstant, size_t pushConstantSize)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    vkCmdPushConstants(cmdBuffer, CurrentPipeline->GetPipelineLayout(),
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    pushConstantSize,
    pushConstant);
        
    BufferAllocation vertexBufferAlloc = bufferAlloc->GetBufferAllocation(vertBufferID);
    VkBuffer vertexBuffer = static_cast<VulkanBufferData*>(vertexBufferAlloc.Buffer)->Buffer;
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
        
    if (indexCount > 0)
    {
        BufferAllocation indexBufferAlloc = bufferAlloc->GetBufferAllocation(indexBufferID);
        VkBuffer indexBuffer = static_cast<VulkanBufferData*>(indexBufferAlloc.Buffer)->Buffer;
        vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(cmdBuffer, vertCount, 1, 0, 0);
    }
}

void VulkanPipelineExecutor::DrawFSQuad()
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
}

void VulkanPipelineExecutor::BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    std::vector<VkDescriptorSet> combinedDescriptorSets;
    std::vector<uint32_t> dynamicOffsets;
    
    if (descriptorSets)
    {
        for (uint64_t ID : *descriptorSets)
        {
            DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(ID);
            combinedDescriptorSets.push_back(reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress));

            dynamicOffsets.insert(dynamicOffsets.end(), 
                                 allocation.DynamicOffsets.begin(), 
                                 allocation.DynamicOffsets.end());
        }
    }
    
    if (combinedDescriptorSets.empty()) return;
    
    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        CurrentPipeline->GetPipelineLayout(),
        numPipelineSets,
        combinedDescriptorSets.size(),
        combinedDescriptorSets.data(),
        dynamicOffsets.size(),           // NEW: Dynamic offset count
        dynamicOffsets.data()             // NEW: Dynamic offsets array
    );
}

void VulkanPipelineExecutor::BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    std::vector<VkDescriptorSet> combinedDescriptorSets;
    std::vector<uint32_t> dynamicOffsets;
    
    if (descriptorSets)
    {
        for (uint64_t ID : *descriptorSets)
        {
            DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(ID);
            combinedDescriptorSets.push_back(reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress));
            
            dynamicOffsets.insert(dynamicOffsets.end(), 
                                 allocation.DynamicOffsets.begin(), 
                                 allocation.DynamicOffsets.end());
        }
    }
        
    for (uint64_t ID : CurrentPipeline->GetInputDescriptorSetIDs())
    {
        DescriptorSetAllocation allocation = bufferAlloc->GetDescriptorSet(ID);
        combinedDescriptorSets.push_back(reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress));
        
        dynamicOffsets.insert(dynamicOffsets.end(), 
                             allocation.DynamicOffsets.begin(), 
                             allocation.DynamicOffsets.end());
    }
    
    if (combinedDescriptorSets.empty()) return;
    
    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        CurrentPipeline->GetPipelineLayout(),
        0,
        combinedDescriptorSets.size(),
        combinedDescriptorSets.data(),
        dynamicOffsets.size(),
        dynamicOffsets.data()
    );
}

VkCommandBuffer VulkanPipelineExecutor::GetCommandBuffer()
{
    return VulkanCore::GetInstance().GetCommandBuffer();
}

