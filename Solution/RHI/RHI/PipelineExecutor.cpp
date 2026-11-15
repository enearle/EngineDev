#include "PipelineExecutor.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include "../DirectX12/D3D12Structs.h"
#include "BufferAllocator.h"
#include "../Window.h"
#include "../Windows/Win32ErrorHandler.h"

using namespace D3D12Structs;
using namespace Win32ErrorHandler;

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

    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    uint32_t slots = D3DCore::Instance().GetFramesInFlight();
    TimerSlots.resize(slots);

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = slots * MaxTimersPerFrame * 2;
    device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&TimestampHeap)) >> ERROR_HANDLER;

    CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(heapDesc.Count * sizeof(uint64_t));
    device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&TimestampReadback)) >> ERROR_HANDLER;

    UINT64 frequency = 1;
    D3DCore::Instance().GetCommandQueue()->GetTimestampFrequency(&frequency) >> ERROR_HANDLER;
    TimestampTicksPerMillisecond = static_cast<double>(frequency) / 1000.0;
}

D3DPipelineExecutor::~D3DPipelineExecutor()
{
    TimestampHeap.Reset();
    TimestampReadback.Reset();
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

bool D3DPipelineExecutor::BeginFrame()
{
    D3DCore& core = D3DCore::Instance();
    core.BeginFrame();
    BufferAllocator::GetInstance()->BeginFrame(core.GetFrameNumber(), core.GetFramesInFlight());
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};

    uint32_t slot = core.GetCurrentFrameIndex();
    TimerSlot& timers = TimerSlots[slot];
    if (!timers.Names.empty())
    {
        uint32_t base = slot * MaxTimersPerFrame * 2;
        D3D12_RANGE range = { base * sizeof(uint64_t), (base + timers.Names.size() * 2) * sizeof(uint64_t) };
        uint64_t* ticks = nullptr;
        TimestampReadback->Map(0, &range, reinterpret_cast<void**>(&ticks)) >> ERROR_HANDLER;
        CompletedTimers.clear();
        for (size_t i = 0; i < timers.Names.size(); i++)
        {
            uint64_t begin = ticks[base + i * 2];
            uint64_t end = ticks[base + i * 2 + 1];
            CompletedTimers.push_back({ timers.Names[i], end > begin ? (end - begin) / TimestampTicksPerMillisecond : 0.0 });
        }
        D3D12_RANGE noWrite = { 0, 0 };
        TimestampReadback->Unmap(0, &noWrite);
        timers.Names.clear();
    }
    return true;
}

void D3DPipelineExecutor::EndFrame()
{
    uint32_t slot = D3DCore::Instance().GetCurrentFrameIndex();
    uint32_t count = static_cast<uint32_t>(TimerSlots[slot].Names.size()) * 2;
    if (count > 0)
    {
        uint32_t base = slot * MaxTimersPerFrame * 2;
        GetCommandList()->ResolveQueryData(TimestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, base, count,
            TimestampReadback.Get(), base * sizeof(uint64_t));
    }
    D3DCore::Instance().EndFrame();
}

uint64_t D3DPipelineExecutor::GetCompletedFrameNumber() const
{
    return D3DCore::Instance().GetCompletedFrameNumber();
}

uint32_t D3DPipelineExecutor::BeginTimer(const std::string& name)
{
    uint32_t slot = D3DCore::Instance().GetCurrentFrameIndex();
    std::vector<std::string>& names = TimerSlots[slot].Names;
    if (names.size() >= MaxTimersPerFrame)
        return UINT32_MAX;

    uint32_t timer = static_cast<uint32_t>(names.size());
    names.push_back(name);
    GetCommandList()->EndQuery(TimestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * MaxTimersPerFrame * 2 + timer * 2);
    return timer;
}

void D3DPipelineExecutor::EndTimer(uint32_t timer)
{
    if (timer == UINT32_MAX)
        return;
    uint32_t slot = D3DCore::Instance().GetCurrentFrameIndex();
    GetCommandList()->EndQuery(TimestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * MaxTimersPerFrame * 2 + timer * 2 + 1);
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

    for (const ImageTransition& transition : transitions)
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(transition.Image);
        ID3D12Resource* resource = static_cast<DX12ImageData*>(allocation.Image)->Image.Get();

        if (transition.From == ImageLayout::Storage && transition.To == ImageLayout::Storage)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resource));
            continue;
        }

        D3D12_RESOURCE_STATES before = ConvertLayoutToResourceState(transition.From);
        D3D12_RESOURCE_STATES after = ConvertLayoutToResourceState(transition.To);
        if (before == after)
            continue;

        SubresourceRange range = ResolveRange(allocation.Desc, transition.Range);
        if (IsWholeImage(allocation.Desc, range))
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after));
            continue;
        }

        uint32_t mipLevels = (std::max)(allocation.Desc.MipLevels, 1u);
        uint32_t arraySize = (std::max)(allocation.Desc.ArrayLayers, 1u);
        for (uint32_t layer = range.BaseLayer; layer < range.BaseLayer + range.LayerCount; layer++)
            for (uint32_t mip = range.BaseMip; mip < range.BaseMip + range.MipCount; mip++)
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after,
                    D3D12CalcSubresource(mip, layer, 0, mipLevels, arraySize)));
    }

    if (!barriers.empty())
        GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

static D3D12_RESOURCE_STATES D3DBufferState(BufferState state)
{
    switch (state)
    {
    case BufferState::ShaderRead:      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case BufferState::ShaderWrite:     return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case BufferState::CopySource:      return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case BufferState::CopyDestination: return D3D12_RESOURCE_STATE_COPY_DEST;
    default:                           return D3D12_RESOURCE_STATE_COMMON;
    }
}

void D3DPipelineExecutor::TransitionBuffers(std::span<const BufferTransition> transitions)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    std::vector<D3D12_RESOURCE_BARRIER> barriers;

    for (const BufferTransition& transition : transitions)
    {
        const BufferAllocation& allocation = allocator->GetBufferAllocation(transition.Buffer);
        if (allocation.IsMapped)
            continue;

        ID3D12Resource* resource = DXBuffer(allocation)->Buffer.Get();
        if (transition.From == BufferState::ShaderWrite && transition.To == BufferState::ShaderWrite)
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resource));
        else if (transition.From != transition.To)
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3DBufferState(transition.From), D3DBufferState(transition.To)));
    }

    if (!barriers.empty())
        GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

static D3D12_TEXTURE_COPY_LOCATION D3DFootprint(ID3D12Resource* buffer, uint64_t offset, uint32_t rowPitch, const ImageDesc& desc, const ImageRegion& region)
{
    D3D12_TEXTURE_COPY_LOCATION location = {};
    location.pResource = buffer;
    location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    location.PlacedFootprint.Offset = offset;
    location.PlacedFootprint.Footprint = { DXFormat(desc.Format), region.Width, region.Height, 1, rowPitch };
    return location;
}

static D3D12_TEXTURE_COPY_LOCATION D3DSubresource(ID3D12Resource* image, const ImageDesc& desc, const ImageRegion& region)
{
    D3D12_TEXTURE_COPY_LOCATION location = {};
    location.pResource = image;
    location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    location.SubresourceIndex = D3D12CalcSubresource(region.Mip, region.Layer, 0, (std::max)(desc.MipLevels, 1u), (std::max)(desc.ArrayLayers, 1u));
    return location;
}

void D3DPipelineExecutor::CopyBufferToImage(BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch, ImageHandle image, const ImageRegion& region)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    const ImageAllocation& imageAllocation = allocator->GetImageAllocation(image);
    D3D12_TEXTURE_COPY_LOCATION source = D3DFootprint(DXBuffer(allocator->GetBufferAllocation(buffer))->Buffer.Get(), bufferOffset, rowPitch, imageAllocation.Desc, region);
    D3D12_TEXTURE_COPY_LOCATION destination = D3DSubresource(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), imageAllocation.Desc, region);
    GetCommandList()->CopyTextureRegion(&destination, region.X, region.Y, 0, &source, nullptr);
}

void D3DPipelineExecutor::CopyImageToBuffer(ImageHandle image, const ImageRegion& region, BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    const ImageAllocation& imageAllocation = allocator->GetImageAllocation(image);
    D3D12_TEXTURE_COPY_LOCATION source = D3DSubresource(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), imageAllocation.Desc, region);
    D3D12_TEXTURE_COPY_LOCATION destination = D3DFootprint(DXBuffer(allocator->GetBufferAllocation(buffer))->Buffer.Get(), bufferOffset, rowPitch, imageAllocation.Desc, region);
    D3D12_BOX box = { region.X, region.Y, 0, region.X + region.Width, region.Y + region.Height, 1 };
    GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
}

void D3DPipelineExecutor::BeginRendering(const RenderingDesc& desc)
{
    ID3D12GraphicsCommandList* cmdList = GetCommandList();
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargets;
    DirectX12BufferAllocator* dxAllocator = static_cast<DirectX12BufferAllocator*>(allocator);
    for (const ColorAttachment& attachment : desc.ColorAttachments)
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(attachment.Image);
        D3D12_CPU_DESCRIPTOR_HANDLE view = dxAllocator->GetRenderTargetView(attachment.Image, attachment.Range);
        renderTargets.push_back(view);
        const DirectX::XMFLOAT4& colour = attachment.HasClearColour ? attachment.ClearColour : allocation.Desc.ClearColour;
        if (attachment.LoadOp == AttachmentLoadOp::Clear)
            cmdList->ClearRenderTargetView(view, &colour.x, 0, nullptr);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE depthView = {};
    if (desc.Depth.Image.IsValid())
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(desc.Depth.Image);
        depthView = dxAllocator->GetDepthStencilView(desc.Depth.Image, desc.Depth.Range);
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
    bool isLayoutChanged = !CurrentPipeline || CurrentPipeline->GetLayout() != pipeline->GetLayout()
        || CurrentPipeline->IsCompute() != pipeline->IsCompute();

    if (isLayoutChanged)
    {
        ID3D12DescriptorHeap* heaps[] = { static_cast<DirectX12BufferAllocator*>(BufferAllocator::GetInstance())->GetShaderResourceHeap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        ID3D12RootSignature* rootSignature = static_cast<D3DPipelineLayout*>(pipeline->GetLayout())->GetRootSignature();
        if (pipeline->IsCompute())
            cmdList->SetComputeRootSignature(rootSignature);
        else
            cmdList->SetGraphicsRootSignature(rootSignature);
    }

    cmdList->SetPipelineState(d3dPipeline->GetPipelineState());
    if (!pipeline->IsCompute())
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

    if (dynamicOffsets.size() != parameters->DynamicParameters.size())
        throw std::runtime_error("Dynamic offset count does not match the set's dynamic uniform buffers");

    bool isCompute = CurrentPipeline->IsCompute();
    if (parameters->TableParameter >= 0)
        isCompute ? cmdList->SetComputeRootDescriptorTable(parameters->TableParameter, table->BaseHandle)
                  : cmdList->SetGraphicsRootDescriptorTable(parameters->TableParameter, table->BaseHandle);

    for (size_t i = 0; i < dynamicOffsets.size(); i++)
    {
        D3D12_GPU_VIRTUAL_ADDRESS address = table->DynamicAddresses[i] + dynamicOffsets[i];
        isCompute ? cmdList->SetComputeRootConstantBufferView(parameters->DynamicParameters[i], address)
                  : cmdList->SetGraphicsRootConstantBufferView(parameters->DynamicParameters[i], address);
    }
}

void D3DPipelineExecutor::PushConstants(const void* data, uint32_t size)
{
    int32_t parameter = static_cast<D3DPipelineLayout*>(CurrentPipeline->GetLayout())->GetConstantsParameter();
    if (parameter < 0)
        return;
    if (CurrentPipeline->IsCompute())
        GetCommandList()->SetComputeRoot32BitConstants(parameter, size / 4, data, 0);
    else
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

void D3DPipelineExecutor::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    GetCommandList()->Dispatch(groupsX, groupsY, groupsZ);
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
        return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT };
    case ImageLayout::Storage:
        return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT };
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

    uint32_t slots = VulkanCore::Instance().GetFramesInFlight();
    TimerSlots.resize(slots);

    VkQueryPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolInfo.queryCount = slots * MaxTimersPerFrame * 2;
    if (vkCreateQueryPool(VulkanCore::Instance().GetDevice(), &poolInfo, nullptr, &TimestampPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create the timestamp query pool.");
    MillisecondsPerTick = VulkanCore::Instance().GetTimestampPeriod() / 1.0e6;
}

VulkanPipelineExecutor::~VulkanPipelineExecutor()
{
    vkDestroyQueryPool(VulkanCore::Instance().GetDevice(), TimestampPool, nullptr);
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

bool VulkanPipelineExecutor::BeginFrame()
{
    VulkanCore& core = VulkanCore::Instance();
    if (core.IsSwapchainOutOfDate() || !core.BeginFrame())
    {
        core.WaitForGpu();
        ResetWindow();
        return false;
    }

    BufferAllocator::GetInstance()->BeginFrame(core.GetFrameNumber(), core.GetFramesInFlight());
    CurrentPipeline = nullptr;
    BoundVertexBuffer = {};
    BoundIndexBuffer = {};

    uint32_t slot = core.GetCurrentFrameIndex();
    uint32_t base = slot * MaxTimersPerFrame * 2;
    TimerSlot& timers = TimerSlots[slot];
    if (!timers.Names.empty())
    {
        std::vector<uint64_t> ticks(timers.Names.size() * 2);
        VkResult result = vkGetQueryPoolResults(core.GetDevice(), TimestampPool, base, static_cast<uint32_t>(ticks.size()),
            ticks.size() * sizeof(uint64_t), ticks.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (result == VK_SUCCESS)
        {
            CompletedTimers.clear();
            for (size_t i = 0; i < timers.Names.size(); i++)
            {
                uint64_t begin = ticks[i * 2];
                uint64_t end = ticks[i * 2 + 1];
                CompletedTimers.push_back({ timers.Names[i], end > begin ? (end - begin) * MillisecondsPerTick : 0.0 });
            }
        }
        timers.Names.clear();
    }
    vkCmdResetQueryPool(GetCommandBuffer(), TimestampPool, base, MaxTimersPerFrame * 2);
    return true;
}

uint64_t VulkanPipelineExecutor::GetCompletedFrameNumber() const
{
    return VulkanCore::Instance().GetCompletedFrameNumber();
}

uint32_t VulkanPipelineExecutor::BeginTimer(const std::string& name)
{
    uint32_t slot = VulkanCore::Instance().GetCurrentFrameIndex();
    std::vector<std::string>& names = TimerSlots[slot].Names;
    if (names.size() >= MaxTimersPerFrame)
        return UINT32_MAX;

    uint32_t timer = static_cast<uint32_t>(names.size());
    names.push_back(name);
    vkCmdWriteTimestamp(GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampPool, slot * MaxTimersPerFrame * 2 + timer * 2);
    return timer;
}

void VulkanPipelineExecutor::EndTimer(uint32_t timer)
{
    if (timer == UINT32_MAX)
        return;
    uint32_t slot = VulkanCore::Instance().GetCurrentFrameIndex();
    vkCmdWriteTimestamp(GetCommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampPool, slot * MaxTimersPerFrame * 2 + timer * 2 + 1);
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
        bool isStorageOrdering = transition.From == ImageLayout::Storage && transition.To == ImageLayout::Storage;
        if (transition.From == transition.To && !isStorageOrdering)
            continue;

        const ImageAllocation& allocation = allocator->GetImageAllocation(transition.Image);
        bool isDepth = IsDepthFormat(allocation.Desc.Format);
        SubresourceRange range = ResolveRange(allocation.Desc, transition.Range);
        VulkanLayoutAccess source = VulkanAccessFor(transition.From, isDepth);
        VulkanLayoutAccess destination = VulkanAccessFor(transition.To, isDepth);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VulkanImageLayout(transition.From);
        barrier.newLayout = VulkanImageLayout(transition.To);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = static_cast<VulkanImageData*>(allocation.Image)->ImageHandle;
        barrier.subresourceRange = { static_cast<VkImageAspectFlags>(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
                                     range.BaseMip, range.MipCount, range.BaseLayer, range.LayerCount };
        barrier.srcAccessMask = isStorageOrdering ? VK_ACCESS_SHADER_WRITE_BIT : source.Access;
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

static VkAccessFlags VulkanBufferAccess(BufferState state)
{
    switch (state)
    {
    case BufferState::ShaderRead:      return VK_ACCESS_SHADER_READ_BIT;
    case BufferState::ShaderWrite:     return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case BufferState::CopySource:      return VK_ACCESS_TRANSFER_READ_BIT;
    case BufferState::CopyDestination: return VK_ACCESS_TRANSFER_WRITE_BIT;
    default:                           return 0;
    }
}

void VulkanPipelineExecutor::TransitionBuffers(std::span<const BufferTransition> transitions)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    std::vector<VkBufferMemoryBarrier> barriers;

    for (const BufferTransition& transition : transitions)
    {
        if (transition.From == transition.To && transition.From != BufferState::ShaderWrite)
            continue;

        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = transition.From == BufferState::ShaderWrite ? VK_ACCESS_SHADER_WRITE_BIT : VulkanBufferAccess(transition.From);
        barrier.dstAccessMask = VulkanBufferAccess(transition.To);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = static_cast<VulkanBufferData*>(allocator->GetBufferAllocation(transition.Buffer).Buffer)->Buffer;
        barrier.size = VK_WHOLE_SIZE;
        barriers.push_back(barrier);
    }

    if (!barriers.empty())
        vkCmdPipelineBarrier(GetCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
            0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data(), 0, nullptr);
}

static VkBufferImageCopy VulkanCopyRegion(uint64_t bufferOffset, uint32_t rowPitch, const ImageDesc& desc, const ImageRegion& region)
{
    VkBufferImageCopy copy{};
    copy.bufferOffset = bufferOffset;
    copy.bufferRowLength = rowPitch / static_cast<uint32_t>(FormatSize(desc.Format));
    copy.imageSubresource = { static_cast<VkImageAspectFlags>(IsDepthFormat(desc.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
                              region.Mip, region.Layer, 1 };
    copy.imageOffset = { static_cast<int32_t>(region.X), static_cast<int32_t>(region.Y), 0 };
    copy.imageExtent = { region.Width, region.Height, 1 };
    return copy;
}

void VulkanPipelineExecutor::CopyBufferToImage(BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch, ImageHandle image, const ImageRegion& region)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    const ImageAllocation& imageAllocation = allocator->GetImageAllocation(image);
    VkBufferImageCopy copy = VulkanCopyRegion(bufferOffset, rowPitch, imageAllocation.Desc, region);
    vkCmdCopyBufferToImage(GetCommandBuffer(), static_cast<VulkanBufferData*>(allocator->GetBufferAllocation(buffer).Buffer)->Buffer,
        static_cast<VulkanImageData*>(imageAllocation.Image)->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

void VulkanPipelineExecutor::CopyImageToBuffer(ImageHandle image, const ImageRegion& region, BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    const ImageAllocation& imageAllocation = allocator->GetImageAllocation(image);
    VkBufferImageCopy copy = VulkanCopyRegion(bufferOffset, rowPitch, imageAllocation.Desc, region);
    vkCmdCopyImageToBuffer(GetCommandBuffer(), static_cast<VulkanImageData*>(imageAllocation.Image)->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        static_cast<VulkanBufferData*>(allocator->GetBufferAllocation(buffer).Buffer)->Buffer, 1, &copy);
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
    VulkanBufferAllocator* vkAllocator = static_cast<VulkanBufferAllocator*>(allocator);

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (const ColorAttachment& attachment : desc.ColorAttachments)
    {
        const ImageAllocation& allocation = allocator->GetImageAllocation(attachment.Image);
        VkRenderingAttachmentInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = vkAllocator->GetImageView(attachment.Image, attachment.Range);
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
        depthAttachment.imageView = vkAllocator->GetImageView(desc.Depth.Image, desc.Depth.Range);
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VulkanLoadOp(desc.Depth.LoadOp);
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { allocation.Desc.ClearDepth, 0 };
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { { 0, 0 }, { desc.Width, desc.Height } };
    renderingInfo.layerCount = 1;
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

static VkPipelineBindPoint VulkanBindPoint(const Pipeline* pipeline)
{
    return pipeline->IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
}

void VulkanPipelineExecutor::BindPipeline(Pipeline* pipeline)
{
    VkCommandBuffer cmdBuffer = GetCommandBuffer();
    VkPipelineBindPoint bindPoint = VulkanBindPoint(pipeline);
    vkCmdBindPipeline(cmdBuffer, bindPoint, static_cast<VulkanPipeline*>(pipeline)->GetVulkanPipeline());

    if (!CurrentPipeline || CurrentPipeline->GetLayout() != pipeline->GetLayout() || CurrentPipeline->IsCompute() != pipeline->IsCompute())
    {
        VkDescriptorSet samplerSet = VulkanCore::Instance().GetGlobalSamplerSet();
        vkCmdBindDescriptorSets(cmdBuffer, bindPoint, static_cast<VulkanPipelineLayout*>(pipeline->GetLayout())->GetLayout(),
            0, 1, &samplerSet, 0, nullptr);
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

    vkCmdBindDescriptorSets(GetCommandBuffer(), VulkanBindPoint(CurrentPipeline),
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

void VulkanPipelineExecutor::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    vkCmdDispatch(GetCommandBuffer(), groupsX, groupsY, groupsZ);
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
