#include "PipelineExecutor.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include "../DirectX12/D3D12Structs.h"
#include "BufferAllocator.h"
#include "../Window.h"

using namespace D3D12Structs;

PipelineExecutor* PipelineExecutor::Create(Window* window, CoreInitData data)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanPipelineExecutor(window, data);
    if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new D3DPipelineExecutor(window, data);
    throw std::runtime_error("Invalid graphics API selected.");
}

void PipelineExecutor::EvictBackBuffers()
{
    for (ImageHandle image : BackBuffers)
        BufferAllocator::GetInstance()->EvictImage(image);
    BackBuffers.clear();
}

//================================================//
// DirectX 12                                     //
//================================================//

D3DPipelineExecutor::D3DPipelineExecutor(Window* window, CoreInitData data)
{
    D3DCore::Instance().InitDirect3D(window, data);
    BackBufferFormat = D3DCore::Instance().GetRenderTargetFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT
        ? Format::R16G16B16A16_FLOAT : Format::R8G8B8A8_UNORM;
    EnlistBackBuffers();
}

D3DPipelineExecutor::~D3DPipelineExecutor()
{
    EvictBackBuffers();
    D3DCore::Instance().Reset();
}

void D3DPipelineExecutor::EnlistBackBuffers()
{
    EvictBackBuffers();
    D3DCore& core = D3DCore::Instance();
    for (uint32_t i = 0; i < core.GetBackBufferCount(); i++)
    {
        DX12ImageData* imageData = new DX12ImageData();
        imageData->Image = core.GetBackBuffer(i);
        imageData->RenderTargetView = core.GetBackBufferDescriptor(i);

        ImageAllocation allocation;
        allocation.Image = imageData;
        allocation.Desc.Width = core.GetWindow()->GetWidth();
        allocation.Desc.Height = core.GetWindow()->GetHeight();
        allocation.Desc.Format = BackBufferFormat;
        allocation.Desc.Usage.RenderTarget = true;
        BackBuffers.push_back(BufferAllocator::GetInstance()->EnlistExternalImage(allocation));
    }
}

void D3DPipelineExecutor::BeginFrame()
{
    D3DCore::Instance().BeginFrame();
    BufferAllocator::GetInstance()->BeginFrame(D3DCore::Instance().GetFrameNumber(), D3DCore::Instance().GetFramesInFlight());
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};
}

void D3DPipelineExecutor::EndFrame()
{
    D3DCore::Instance().EndFrame();
}

void D3DPipelineExecutor::Wait()
{
    D3DCore::Instance().WaitForGpu();
}

uint64_t D3DPipelineExecutor::GetFrameNumber() const
{
    return D3DCore::Instance().GetFrameNumber();
}

ImageHandle D3DPipelineExecutor::GetBackBuffer() const
{
    return BackBuffers[D3DCore::Instance().GetCurrentFrameIndex()];
}

ID3D12GraphicsCommandList* D3DPipelineExecutor::GetCommandList()
{
    return D3DCore::Instance().GetCommandList().Get();
}

void D3DPipelineExecutor::TransitionImages(std::span<const ImageTransition> transitions)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(transitions.size());

    for (const ImageTransition& transition : transitions)
    {
        D3D12_RESOURCE_STATES before = ConvertLayoutToResourceState(transition.From);
        D3D12_RESOURCE_STATES after = ConvertLayoutToResourceState(transition.To);
        if (before == after)
            continue;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<DX12ImageData*>(allocator->GetImageAllocation(transition.Image).Image)->Image.Get();
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers.push_back(barrier);
    }

    if (!barriers.empty())
        GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

void D3DPipelineExecutor::BeginRendering(const RenderingDesc& desc)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargets;
    for (const ColorAttachment& attachment : desc.ColorAttachments)
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(attachment.Image);
        D3D12_CPU_DESCRIPTOR_HANDLE view = static_cast<DX12ImageData*>(allocation.Image)->RenderTargetView;
        renderTargets.push_back(view);
        const DirectX::XMFLOAT4& colour = attachment.HasClearColour ? attachment.ClearColour : allocation.Desc.ClearColour;
        if (attachment.LoadOp == AttachmentLoadOp::Clear)
            cmdList->ClearRenderTargetView(view, &colour.x, 0, nullptr);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE depthView = {};
    if (desc.Depth.Image.IsValid())
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(desc.Depth.Image);
        depthView = static_cast<DX12ImageData*>(allocation.Image)->DepthStencilView;
        if (desc.Depth.LoadOp == AttachmentLoadOp::Clear)
            cmdList->ClearDepthStencilView(depthView, D3D12_CLEAR_FLAG_DEPTH, allocation.Desc.ClearDepth, 0, 0, nullptr);
    }

    cmdList->OMSetRenderTargets(static_cast<UINT>(renderTargets.size()), renderTargets.empty() ? nullptr : renderTargets.data(),
        FALSE, desc.Depth.Image.IsValid() ? &depthView : nullptr);
    SetViewport(0, 0, desc.Width, desc.Height);
}

void D3DPipelineExecutor::EndRendering()
{
}

void D3DPipelineExecutor::BindPipeline(Pipeline* pipeline)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    D3DPipeline* d3dPipeline = static_cast<D3DPipeline*>(pipeline);

    if (!CurrentPipeline || CurrentPipeline->GetLayout() != pipeline->GetLayout())
    {
        ID3D12DescriptorHeap* heaps[] = { static_cast<DirectX12BufferAllocator*>(BufferAllocator::GetInstance())->GetShaderResourceHeap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        cmdList->SetGraphicsRootSignature(static_cast<D3DPipelineLayout*>(pipeline->GetLayout())->GetRootSignature());
    }

    cmdList->SetPipelineState(d3dPipeline->GetPipelineState());
    cmdList->IASetPrimitiveTopology(d3dPipeline->GetTopology());
    CurrentPipeline = pipeline;
}

void D3DPipelineExecutor::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    D3D12_VIEWPORT viewport = { static_cast<FLOAT>(x), static_cast<FLOAT>(y), static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0f, 1.0f };
    D3D12_RECT scissor = { static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + width), static_cast<LONG>(y + height) };
    GetCommandList()->RSSetViewports(1, &viewport);
    GetCommandList()->RSSetScissorRects(1, &scissor);
}

void D3DPipelineExecutor::BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets)
{
    const DescriptorSetAllocation& allocation = BufferAllocator::GetInstance()->GetDescriptorSet(set);
    uint32_t setIndex = static_cast<uint32_t>(allocation.SetKey & 0xFFFFFFFF);

    const D3DPipelineLayout::SetParameters* parameters = static_cast<D3DPipelineLayout*>(CurrentPipeline->GetLayout())->FindSet(setIndex);
    if (!parameters)
        throw std::runtime_error("Descriptor set " + std::to_string(setIndex) + " is not in the bound pipeline's layout");

    const DirectX12BufferAllocator::DescriptorTableData* table = static_cast<DirectX12BufferAllocator::DescriptorTableData*>(allocation.PlatformData);
    ID3D12GraphicsCommandList* cmdList = GetCommandList();

    if (parameters->TableParameter >= 0)
        cmdList->SetGraphicsRootDescriptorTable(parameters->TableParameter, table->BaseHandle);

    if (dynamicOffsets.size() != parameters->DynamicParameters.size())
        throw std::runtime_error("Dynamic offset count does not match the set's dynamic uniform buffers");

    for (size_t i = 0; i < dynamicOffsets.size(); i++)
        cmdList->SetGraphicsRootConstantBufferView(parameters->DynamicParameters[i], table->DynamicAddresses[i] + dynamicOffsets[i]);
}

void D3DPipelineExecutor::PushConstants(const void* data, uint32_t size)
{
    int32_t parameter = static_cast<D3DPipelineLayout*>(CurrentPipeline->GetLayout())->GetConstantsParameter();
    if (parameter >= 0)
        GetCommandList()->SetGraphicsRoot32BitConstants(parameter, size / 4, data, 0);
}

void D3DPipelineExecutor::DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                                      uint32_t indexCount, uint32_t instanceCount)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    if (vertexBuffer != BoundVertexBuffer)
    {
        const BufferAllocation& allocation = allocator->GetBufferAllocation(vertexBuffer);
        D3D12_VERTEX_BUFFER_VIEW view = { DXBuffer(allocation)->GPUAddress, static_cast<UINT>(allocation.Size), vertexStride };
        cmdList->IASetVertexBuffers(0, 1, &view);
        BoundVertexBuffer = vertexBuffer;
    }

    if (indexBuffer != BoundIndexBuffer)
    {
        const BufferAllocation& allocation = allocator->GetBufferAllocation(indexBuffer);
        D3D12_INDEX_BUFFER_VIEW view = { DXBuffer(allocation)->GPUAddress, static_cast<UINT>(allocation.Size), DXGI_FORMAT_R32_UINT };
        cmdList->IASetIndexBuffer(&view);
        BoundIndexBuffer = indexBuffer;
    }

    cmdList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
}

void D3DPipelineExecutor::Draw(uint32_t vertexCount, uint32_t instanceCount)
{
    GetCommandList()->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void D3DPipelineExecutor::BeginOverlayPass(uint32_t width, uint32_t height)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = D3DCore::Instance().GetRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3DCore::Instance().GetOverlayDepthStencilDescriptor();

    cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtv, false, &dsv);
    SetViewport(0, 0, width, height);
}

void D3DPipelineExecutor::EndOverlayPass()
{
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};
}

void D3DPipelineExecutor::ResetWindow()
{
    EvictBackBuffers();
    D3DCore::Instance().ResetWindow();
    EnlistBackBuffers();
}

//================================================//
// Vulkan                                         //
//================================================//

struct VulkanLayoutAccess
{
    VkPipelineStageFlags Stages;
    VkAccessFlags Access;
};

static VulkanLayoutAccess VulkanAccessFor(ImageLayout layout, bool isDepth)
{
    switch (layout)
    {
    case ImageLayout::Undefined:
        return { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 };
    case ImageLayout::ColorAttachment:
        return { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
    case ImageLayout::DepthStencilAttachment:
        return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
    case ImageLayout::DepthStencilReadOnly:
        return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT };
    case ImageLayout::ShaderReadOnly:
        return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT };
    case ImageLayout::TransferSrc:
        return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT };
    case ImageLayout::TransferDst:
        return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT };
    case ImageLayout::Present:
        return { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0 };
    default:
        return { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
    }
}

VulkanPipelineExecutor::VulkanPipelineExecutor(Window* window, CoreInitData data)
{
    VulkanCore::Instance().InitVulkan(window, data);
    BackBufferFormat = VulkanCore::Instance().GetSwapchainFormat() == VK_FORMAT_B8G8R8A8_UNORM
        ? Format::B8G8R8A8_UNORM : Format::R8G8B8A8_UNORM;
    EnlistBackBuffers();
}

VulkanPipelineExecutor::~VulkanPipelineExecutor()
{
    EvictBackBuffers();
    VulkanCore::Instance().Cleanup();
}

void VulkanPipelineExecutor::EnlistBackBuffers()
{
    EvictBackBuffers();
    VulkanCore& core = VulkanCore::Instance();
    for (const VulkanImageData& swapchainImage : core.GetSwapchainImages())
    {
        ImageAllocation allocation;
        allocation.Image = new VulkanImageData(swapchainImage);
        allocation.Desc.Width = core.GetExtent().width;
        allocation.Desc.Height = core.GetExtent().height;
        allocation.Desc.Format = BackBufferFormat;
        allocation.Desc.Usage.RenderTarget = true;
        BackBuffers.push_back(BufferAllocator::GetInstance()->EnlistExternalImage(allocation));
    }
}

void VulkanPipelineExecutor::BeginFrame()
{
    VulkanCore::Instance().BeginFrame();
    BufferAllocator::GetInstance()->BeginFrame(VulkanCore::Instance().GetFrameNumber(), VulkanCore::Instance().GetFramesInFlight());
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};
}

void VulkanPipelineExecutor::EndFrame()
{
    VulkanCore::Instance().EndFrame();
}

void VulkanPipelineExecutor::Wait()
{
    VulkanCore::Instance().WaitForGpu();
}

uint64_t VulkanPipelineExecutor::GetFrameNumber() const
{
    return VulkanCore::Instance().GetFrameNumber();
}

ImageHandle VulkanPipelineExecutor::GetBackBuffer() const
{
    return BackBuffers[VulkanCore::Instance().GetCurrentSwapchainImageIndex()];
}

VkCommandBuffer VulkanPipelineExecutor::GetCommandBuffer()
{
    return VulkanCore::Instance().GetCommandBuffer();
}

void VulkanPipelineExecutor::TransitionImages(std::span<const ImageTransition> transitions)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    std::vector<VkImageMemoryBarrier> barriers;
    VkPipelineStageFlags sourceStages = 0;
    VkPipelineStageFlags destinationStages = 0;

    for (const ImageTransition& transition : transitions)
    {
        if (transition.From == transition.To)
            continue;

        const ImageAllocation& allocation = allocator->GetImageAllocation(transition.Image);
        bool isDepth = IsDepthFormat(allocation.Desc.Format);
        VulkanLayoutAccess source = VulkanAccessFor(transition.From, isDepth);
        VulkanLayoutAccess destination = VulkanAccessFor(transition.To, isDepth);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VulkanImageLayout(transition.From);
        barrier.newLayout = VulkanImageLayout(transition.To);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = static_cast<VulkanImageData*>(allocation.Image)->ImageHandle;
        barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        barrier.srcAccessMask = source.Access;
        barrier.dstAccessMask = destination.Access;
        barriers.push_back(barrier);

        sourceStages |= source.Stages;
        destinationStages |= destination.Stages;
    }

    if (barriers.empty())
        return;

    vkCmdPipelineBarrier(GetCommandBuffer(), sourceStages, destinationStages, 0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data());
}

static VkAttachmentLoadOp VulkanLoadOp(AttachmentLoadOp loadOp)
{
    switch (loadOp)
    {
    case AttachmentLoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    default:                         return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
}

void VulkanPipelineExecutor::BeginRendering(const RenderingDesc& desc)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (const ColorAttachment& attachment : desc.ColorAttachments)
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(attachment.Image);
        VkRenderingAttachmentInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = static_cast<VulkanImageData*>(allocation.Image)->ImageView;
        info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        info.loadOp = VulkanLoadOp(attachment.LoadOp);
        info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        const DirectX::XMFLOAT4& colour = attachment.HasClearColour ? attachment.ClearColour : allocation.Desc.ClearColour;
        info.clearValue.color = { { colour.x, colour.y, colour.z, colour.w } };
        colorAttachments.push_back(info);
    }

    VkRenderingAttachmentInfo depthAttachment{};
    if (desc.Depth.Image.IsValid())
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(desc.Depth.Image);
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = static_cast<VulkanImageData*>(allocation.Image)->ImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VulkanLoadOp(desc.Depth.LoadOp);
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { allocation.Desc.ClearDepth, 0 };
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { { 0, 0 }, { desc.Width, desc.Height } };
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = desc.ViewMask;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.empty() ? nullptr : colorAttachments.data();
    renderingInfo.pDepthAttachment = desc.Depth.Image.IsValid() ? &depthAttachment : nullptr;

    vkCmdBeginRendering(GetCommandBuffer(), &renderingInfo);
    SetViewport(0, 0, desc.Width, desc.Height);
}

void VulkanPipelineExecutor::EndRendering()
{
    vkCmdEndRendering(GetCommandBuffer());
}

void VulkanPipelineExecutor::BindPipeline(Pipeline* pipeline)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VulkanPipeline*>(pipeline)->GetVulkanPipeline());

    if (!CurrentPipeline || CurrentPipeline->GetLayout() != pipeline->GetLayout())
    {
        VkDescriptorSet samplerSet = VulkanCore::Instance().GetGlobalSamplerSet();
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            static_cast<VulkanPipelineLayout*>(pipeline->GetLayout())->GetLayout(), 0, 1, &samplerSet, 0, nullptr);
    }
    CurrentPipeline = pipeline;
}

void VulkanPipelineExecutor::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    VkViewport viewport{};
    viewport.x = static_cast<float>(x);
    viewport.y = static_cast<float>(y + height);
    viewport.width = static_cast<float>(width);
    viewport.height = -static_cast<float>(height);
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { { static_cast<int32_t>(x), static_cast<int32_t>(y) }, { width, height } };
    vkCmdSetViewport(GetCommandBuffer(), 0, 1, &viewport);
    vkCmdSetScissor(GetCommandBuffer(), 0, 1, &scissor);
}

void VulkanPipelineExecutor::BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets)
{
    const DescriptorSetAllocation& allocation = BufferAllocator::GetInstance()->GetDescriptorSet(set);
    uint32_t setIndex = static_cast<uint32_t>(allocation.SetKey & 0xFFFFFFFF);
    VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress);

    vkCmdBindDescriptorSets(GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        static_cast<VulkanPipelineLayout*>(CurrentPipeline->GetLayout())->GetLayout(), setIndex, 1, &descriptorSet,
        static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.empty() ? nullptr : dynamicOffsets.data());
}

void VulkanPipelineExecutor::PushConstants(const void* data, uint32_t size)
{
    VulkanPipelineLayout* layout = static_cast<VulkanPipelineLayout*>(CurrentPipeline->GetLayout());
    vkCmdPushConstants(GetCommandBuffer(), layout->GetLayout(), layout->GetPushConstantStages(), 0, size, data);
}

void VulkanPipelineExecutor::DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                                         uint32_t indexCount, uint32_t instanceCount)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    if (vertexBuffer != BoundVertexBuffer)
    {
        VkBuffer buffer = static_cast<VulkanBufferData*>(allocator->GetBufferAllocation(vertexBuffer).Buffer)->Buffer;
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
        BoundVertexBuffer = vertexBuffer;
    }

    if (indexBuffer != BoundIndexBuffer)
    {
        VkBuffer buffer = static_cast<VulkanBufferData*>(allocator->GetBufferAllocation(indexBuffer).Buffer)->Buffer;
        vkCmdBindIndexBuffer(cmdBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);
        BoundIndexBuffer = indexBuffer;
    }

    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, 0, 0, 0);
}

void VulkanPipelineExecutor::Draw(uint32_t vertexCount, uint32_t instanceCount)
{
    vkCmdDraw(GetCommandBuffer(), vertexCount, instanceCount, 0, 0);
}

void VulkanPipelineExecutor::BeginOverlayPass(uint32_t width, uint32_t height)
{
    VkClearValue clearValues[2];
    clearValues[0].depthStencil = { 0.0f, 0 };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = VulkanCore::Instance().GetOverlayRenderPass();
    beginInfo.framebuffer = VulkanCore::Instance().GetCurrentOverlayFramebuffer();
    beginInfo.renderArea = { { 0, 0 }, { width, height } };
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(GetCommandBuffer(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanPipelineExecutor::EndOverlayPass()
{
    vkCmdEndRenderPass(GetCommandBuffer());
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};
}

void VulkanPipelineExecutor::ResetWindow()
{
    EvictBackBuffers();
    VulkanCore::Instance().ResetWindow();
    EnlistBackBuffers();
}
