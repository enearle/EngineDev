#include "Readback.h"

#include <algorithm>
#include <climits>
#include <stdexcept>
#include "Renderer.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"

using namespace RHIStructures;

struct PooledBuffer
{
    BufferHandle Buffer = {};
    uint64_t Size = 0;
    bool IsInUse = false;
};

struct PendingRequest
{
    GraphImageID Image = 0;
    ImageRegion Region = {};
    ReadbackCallback Callback;
};

struct PendingBufferRequest
{
    BufferHandle Buffer = {};
    GraphBufferID GraphBuffer = UINT32_MAX;     // When set, Buffer is looked up in the graph at record time
    BufferState State = BufferState::Undefined;
    uint64_t Offset = 0;
    uint64_t Size = 0;
    ReadbackCallback Callback;
};

struct InFlightCopy
{
    uint64_t Frame = 0;
    size_t PoolIndex = 0;
    uint32_t RowPitch = 0;
    uint64_t Size = 0;
    ImageRegion Region = {};
    ReadbackCallback Callback;
};

static std::vector<PooledBuffer> Pool;
static std::vector<GraphImageID> WatchedImages;
static std::vector<GraphBufferID> WatchedBuffers;
static std::vector<PendingRequest> Pending;
static std::vector<PendingBufferRequest> PendingBuffers;
static std::vector<InFlightCopy> InFlight;
static PassID ReadbackPass = UINT32_MAX;

static size_t ClaimBuffer(uint64_t size)
{
    for (size_t i = 0; i < Pool.size(); i++)
        if (!Pool[i].IsInUse && Pool[i].Size >= size)
        {
            Pool[i].IsInUse = true;
            return i;
        }

    BufferDesc desc;
    desc.Size = size;
    desc.Usage.TransferDestination = true;
    desc.Usage.Access = ReadbackMemory;
    Pool.push_back({ BufferAllocator::GetInstance()->CreateBuffer(desc), size, true });
    return Pool.size() - 1;
}

static void RecordCopies(const PassContext& context)
{
    PipelineExecutor* executor = context.Executor;
    PassGraph& graph = Renderer::GetPassGraph();

    for (PendingRequest& request : Pending)
    {
        ImageHandle image = graph.GetImage(request.Image);
        const ImageDesc& desc = BufferAllocator::GetInstance()->GetImageAllocation(image).Desc;
        uint32_t rowPitch = CopyRowPitch(desc.Format, request.Region.Width);
        size_t poolIndex = ClaimBuffer(static_cast<uint64_t>(rowPitch) * request.Region.Height);

        SubresourceRange range = { request.Region.Mip, 1, request.Region.Layer, 1 };
        ImageTransition toCopy = { image, ImageLayout::ShaderReadOnly, ImageLayout::TransferSrc, range };
        executor->TransitionImages({ &toCopy, 1 });
        executor->CopyImageToBuffer(image, request.Region, Pool[poolIndex].Buffer, 0, rowPitch);
        ImageTransition back = { image, ImageLayout::TransferSrc, ImageLayout::ShaderReadOnly, range };
        executor->TransitionImages({ &back, 1 });

        uint64_t size = static_cast<uint64_t>(rowPitch) * request.Region.Height;
        InFlight.push_back({ executor->GetFrameNumber(), poolIndex, rowPitch, size, request.Region, std::move(request.Callback) });
    }
    Pending.clear();

    for (PendingBufferRequest& request : PendingBuffers)
    {
        if (request.GraphBuffer != UINT32_MAX)
        {
            request.Buffer = graph.GetBuffer(request.GraphBuffer);
            if (!request.Buffer.IsValid())
                continue;
        }
        size_t poolIndex = ClaimBuffer(request.Size);
        BufferTransition toCopy = { request.Buffer, request.State, BufferState::CopySource };
        executor->TransitionBuffers({ &toCopy, 1 });
        executor->CopyBuffer(request.Buffer, request.Offset, Pool[poolIndex].Buffer, 0, request.Size);
        BufferTransition back = { request.Buffer, BufferState::CopySource, request.State };
        executor->TransitionBuffers({ &back, 1 });

        InFlight.push_back({ executor->GetFrameNumber(), poolIndex, 0, request.Size, {}, std::move(request.Callback) });
    }
    PendingBuffers.clear();
}

static void RebuildPass()
{
    PassGraph& graph = Renderer::GetPassGraph();
    if (ReadbackPass != UINT32_MAX)
        graph.RemovePass(ReadbackPass);

    PassDesc desc;
    desc.Name = "Readback";
    desc.Kind = PassKind::Compute;
    desc.Order = INT_MAX;
    for (GraphImageID image : WatchedImages)
        desc.SampledReads.push_back({ image, 0 });
    for (GraphBufferID buffer : WatchedBuffers)
        desc.BufferReads.push_back({ buffer, BufferState::CopySource });
    desc.Execute = RecordCopies;
    ReadbackPass = graph.AddPass(std::move(desc));
}

void Readback::Start()
{
    RebuildPass();
}

void Readback::Shutdown()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    for (PooledBuffer& buffer : Pool)
        allocator->DestroyBuffer(buffer.Buffer);
    Pool.clear();
    WatchedImages.clear();
    WatchedBuffers.clear();
    Pending.clear();
    PendingBuffers.clear();
    InFlight.clear();
    ReadbackPass = UINT32_MAX;
}

void Readback::Watch(GraphImageID image)
{
    if (std::find(WatchedImages.begin(), WatchedImages.end(), image) != WatchedImages.end())
        return;
    WatchedImages.push_back(image);
    RebuildPass();
}

void Readback::Unwatch(GraphImageID image)
{
    std::erase(WatchedImages, image);
    std::erase_if(Pending, [image](const PendingRequest& request) { return request.Image == image; });
    RebuildPass();
}

void Readback::Request(GraphImageID image, const ImageRegion& region, ReadbackCallback callback)
{
    if (std::find(WatchedImages.begin(), WatchedImages.end(), image) == WatchedImages.end())
        throw std::runtime_error("Readback::Request on an image that is not watched.");
    Pending.push_back({ image, region, std::move(callback) });
}

void Readback::RequestBuffer(BufferHandle buffer, BufferState state, uint64_t offset, uint64_t size, ReadbackCallback callback)
{
    if (size == 0)
        throw std::runtime_error("Readback::RequestBuffer with no bytes.");
    PendingBuffers.push_back({ buffer, UINT32_MAX, state, offset, size, std::move(callback) });
}

void Readback::WatchBuffer(GraphBufferID buffer)
{
    if (std::find(WatchedBuffers.begin(), WatchedBuffers.end(), buffer) != WatchedBuffers.end())
        return;
    WatchedBuffers.push_back(buffer);
    RebuildPass();
}

void Readback::UnwatchBuffer(GraphBufferID buffer)
{
    std::erase(WatchedBuffers, buffer);
    std::erase_if(PendingBuffers, [buffer](const PendingBufferRequest& request) { return request.GraphBuffer == buffer; });
    RebuildPass();
}

void Readback::RequestBuffer(GraphBufferID buffer, uint64_t offset, uint64_t size, ReadbackCallback callback)
{
    if (std::find(WatchedBuffers.begin(), WatchedBuffers.end(), buffer) == WatchedBuffers.end())
        throw std::runtime_error("Readback::RequestBuffer on a graph buffer that is not watched.");
    if (size == 0)
        throw std::runtime_error("Readback::RequestBuffer with no bytes.");
    PendingBufferRequest request;
    request.GraphBuffer = buffer;
    request.State = BufferState::CopySource;
    request.Offset = offset;
    request.Size = size;
    request.Callback = std::move(callback);
    PendingBuffers.push_back(std::move(request));
}

void Readback::DeliverCompleted()
{
    PipelineExecutor* executor = Renderer::GetExecutor();
    uint64_t completed = executor->GetCompletedFrameNumber();
    uint64_t current = executor->GetFrameNumber();

    for (size_t i = 0; i < InFlight.size();)
    {
        InFlightCopy& copy = InFlight[i];
        if (copy.Frame > completed)
        {
            i++;
            continue;
        }

        PooledBuffer& buffer = Pool[copy.PoolIndex];
        ReadbackResult result;
        result.Data = static_cast<const uint8_t*>(BufferAllocator::GetInstance()->GetBufferAllocation(buffer.Buffer).Address);
        result.Size = copy.Size;
        result.RowPitch = copy.RowPitch;
        result.Width = copy.Region.Width;
        result.Height = copy.Region.Height;
        result.RequestedFrame = copy.Frame;
        result.DeliveredFrame = current;
        copy.Callback(result);

        buffer.IsInUse = false;
        InFlight.erase(InFlight.begin() + i);
    }
}
