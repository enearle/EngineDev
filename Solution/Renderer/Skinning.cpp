#include "Skinning.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "MeshRegistry.h"
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "RHI/BufferAllocator.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"

using namespace RHIStructures;

// cs_skinning reads vertices as 32-bit words at these offsets.
static_assert(sizeof(Vertex) == 56 && offsetof(Vertex, Normal) == 12 && offsetof(Vertex, Tangent) == 24 &&
              offsetof(Vertex, Bitangent) == 36 && offsetof(Vertex, TexCoord) == 48);
static_assert(Skinning::MaxPoolVertices == (1ull << 27) * 4 / sizeof(Vertex));
static_assert(sizeof(SkinnedVertex) == 88 && offsetof(SkinnedVertex, Normal) == 12 && offsetof(SkinnedVertex, Tangent) == 24 &&
              offsetof(SkinnedVertex, Bitangent) == 36 && offsetof(SkinnedVertex, TexCoord) == 48 &&
              offsetof(SkinnedVertex, BoneWeights) == 56 && offsetof(SkinnedVertex, BoneIndices) == 72);

static constexpr uint32_t GroupSize = 64;
static constexpr uint32_t SkinningSetIndex = 1;
static constexpr uint32_t MinimumPoolVertices = 4096;

struct SkinningConstants
{
    uint32_t VertexCount;
    uint32_t DestinationBaseVertex;
    uint32_t PaletteIndex;
    uint32_t Padding;
};

struct SkinnedRecord
{
    MeshHandle Source = {};
    uint32_t BaseVertex = 0;
    uint32_t VertexCount = 0;
    DescriptorSetHandle Set = {};
};

struct VertexRange
{
    uint32_t Start = 0;
    uint32_t Count = 0;
};

struct SkinningRequest
{
    SkinnedMeshHandle Mesh = {};
    uint32_t PaletteIndex = 0;
};

static HandlePool<SkinnedRecord, SkinnedMeshHandle> Records;
static std::vector<VertexRange> FreeRanges;     // Sorted by start, never adjacent
static std::vector<SkinningRequest> Requests;
static BufferHandle Pool = {};
static uint32_t PoolCapacity = 0;
static GraphBufferID PoolID = 0;
static PassID SkinningPass = UINT32_MAX;
static PipelineLayout* SkinningLayout = nullptr;
static Pipeline* SkinningPipeline = nullptr;
static uint32_t SkinnedVertexCount = 0;

static void ReturnRange(VertexRange range)
{
    auto next = std::lower_bound(FreeRanges.begin(), FreeRanges.end(), range.Start,
        [](const VertexRange& free, uint32_t start) { return free.Start < start; });
    next = FreeRanges.insert(next, range);
    if (next + 1 != FreeRanges.end() && next->Start + next->Count == (next + 1)->Start)
    {
        next->Count += (next + 1)->Count;
        FreeRanges.erase(next + 1);
    }
    if (next != FreeRanges.begin() && (next - 1)->Start + (next - 1)->Count == next->Start)
    {
        (next - 1)->Count += next->Count;
        FreeRanges.erase(next);
    }
}

static void WriteDescriptorSet(SkinnedRecord& record)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    if (record.Set.IsValid())
        allocator->FreeDescriptorSet(record.Set);
    record.Set = allocator->AllocateDescriptorSet(SkinningLayout->GetID(), SkinningSetIndex, {
        DescriptorSetBinding::ForBuffer(0, MeshRegistry::Get(record.Source).VertexBuffer),
        DescriptorSetBinding::ForBuffer(1, allocator->GetFrameRing()),
        DescriptorSetBinding::ForBuffer(2, Pool),
    });
}

// A larger pool replaces the old one, which is released once the frames using it are done.
static void GrowPool(uint32_t neededVertices)
{
    if (static_cast<uint64_t>(PoolCapacity) + neededVertices > Skinning::MaxPoolVertices)
        throw std::runtime_error("The skinning pool is full: " + std::to_string(PoolCapacity) + " vertices, " +
                                 std::to_string(neededVertices) + " more wanted.");
    uint32_t capacity = (std::max)({ PoolCapacity * 2, PoolCapacity + neededVertices, MinimumPoolVertices });
    capacity = (std::min)(capacity, Skinning::MaxPoolVertices);
    BufferAllocator* allocator = BufferAllocator::GetInstance();

    BufferDesc desc;
    desc.Size = static_cast<uint64_t>(capacity) * sizeof(Vertex);
    desc.Usage.Vertex = true;
    desc.Usage.Storage = true;
    desc.Usage.TransferSource = true;
    desc.Usage.Access = DeviceLocalMemory;
    BufferHandle pool = allocator->CreateBuffer(desc);

    if (Pool.IsValid())
        allocator->DestroyBuffer(Pool);
    Pool = pool;
    ReturnRange({ PoolCapacity, capacity - PoolCapacity });
    PoolCapacity = capacity;
    Renderer::GetPassGraph().SetBuffer(PoolID, Pool);

    Records.ForEachLive([](SkinnedRecord& record) { WriteDescriptorSet(record); });
}

static void ExecuteSkinning(const PassContext& context)
{
    SkinnedVertexCount = 0;
    if (Requests.empty())
        return;

    PipelineExecutor* executor = context.Executor;
    executor->BindPipeline(SkinningPipeline);
    for (const SkinningRequest& request : Requests)
    {
        if (!Records.IsLive(request.Mesh))
            continue;
        const SkinnedRecord& record = Records.Get(request.Mesh);
        if (!MeshRegistry::IsLive(record.Source))
            continue;

        executor->BindDescriptorSet(record.Set);
        SkinningConstants constants = { record.VertexCount, record.BaseVertex, request.PaletteIndex, 0 };
        executor->PushConstants(&constants, sizeof(constants));
        executor->Dispatch((record.VertexCount + GroupSize - 1) / GroupSize, 1, 1);
        SkinnedVertexCount += record.VertexCount;
    }
    Requests.clear();
}

void Skinning::Start()
{
    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);

    ResourceLayout resources;
    resources.VisibleStages = computeStage;
    resources.Bindings = {
        { .Type = DescriptorType::StorageBuffer, .Slot = 0, .Set = SkinningSetIndex, .Count = 1, .Stride = 4 },
        { .Type = DescriptorType::StorageBuffer, .Slot = 1, .Set = SkinningSetIndex, .Count = 1, .Stride = sizeof(DirectX::XMFLOAT4X4) },
        { .Type = DescriptorType::ReadWriteStorageBuffer, .Slot = 2, .Set = SkinningSetIndex, .Count = 1, .Stride = 4 },
    };
    SkinningLayout = PipelineLayout::Create(resources, { { .Size = sizeof(SkinningConstants), .VisibleStages = computeStage } });

    ShaderStage shader = LoadShader("cs_skinning");
    SkinningPipeline = Pipeline::CreateCompute(SkinningLayout, shader);
    ReleaseShader(shader);

    PassGraph& graph = Renderer::GetPassGraph();
    PoolID = graph.ImportBuffer({});

    PassDesc desc;
    desc.Name = "Skinning";
    desc.Kind = PassKind::Compute;
    desc.Order = INT_MIN + 1;
    desc.BufferWrites = { PoolID };
    desc.Execute = ExecuteSkinning;
    SkinningPass = graph.AddPass(std::move(desc));
}

void Skinning::Shutdown()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    Records.ForEachLive([allocator](SkinnedRecord& record) { allocator->FreeDescriptorSet(record.Set); });
    Records.Clear();
    FreeRanges.clear();
    Requests.clear();
    if (Pool.IsValid())
        allocator->DestroyBuffer(Pool);
    Pool = {};
    PoolCapacity = 0;

    delete SkinningPipeline;
    delete SkinningLayout;
    SkinningPipeline = nullptr;
    SkinningLayout = nullptr;
    SkinningPass = UINT32_MAX;
}

SkinnedMeshHandle Skinning::Enlist(MeshHandle sourceMesh)
{
    if (!MeshRegistry::IsLive(sourceMesh) || MeshRegistry::Get(sourceMesh).Layout != VertexLayout::Skinned)
        throw std::runtime_error("Skinning::Enlist needs a live mesh with the Skinned vertex layout.");

    SkinnedRecord record;
    record.Source = sourceMesh;
    record.VertexCount = MeshRegistry::Get(sourceMesh).VertexCount;

    auto fits = [&record](const VertexRange& range) { return range.Count >= record.VertexCount; };
    auto found = std::find_if(FreeRanges.begin(), FreeRanges.end(), fits);
    if (found == FreeRanges.end())
    {
        GrowPool(record.VertexCount);
        found = std::find_if(FreeRanges.begin(), FreeRanges.end(), fits);
    }
    record.BaseVertex = found->Start;
    found->Start += record.VertexCount;
    found->Count -= record.VertexCount;
    if (found->Count == 0)
        FreeRanges.erase(found);

    WriteDescriptorSet(record);
    return Records.Claim(record);
}

void Skinning::Retire(SkinnedMeshHandle mesh)
{
    if (!Records.IsLive(mesh))
        return;
    SkinnedRecord& record = Records.Get(mesh);
    if (record.Set.IsValid())
        BufferAllocator::GetInstance()->FreeDescriptorSet(record.Set);
    ReturnRange({ record.BaseVertex, record.VertexCount });
    Records.Release(mesh);
}

bool Skinning::IsLive(SkinnedMeshHandle mesh)
{
    return Records.IsLive(mesh);
}

MeshHandle Skinning::GetSourceMesh(SkinnedMeshHandle mesh)
{
    return Records.Get(mesh).Source;
}

uint32_t Skinning::GetBaseVertex(SkinnedMeshHandle mesh)
{
    return Records.Get(mesh).BaseVertex;
}

void Skinning::Request(SkinnedMeshHandle mesh, uint32_t paletteIndex)
{
    Requests.push_back({ mesh, paletteIndex });
}

GraphBufferID Skinning::GetPool()
{
    return PoolID;
}

uint32_t Skinning::GetSkinnedVertexCount()
{
    return SkinnedVertexCount;
}
