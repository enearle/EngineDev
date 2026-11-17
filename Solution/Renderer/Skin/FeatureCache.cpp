#include "FeatureCache.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "RHI/BufferAllocator.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"

using namespace RHIStructures;
using DirectX::XMUINT4;

static constexpr uint32_t FeatureSetIndex = 1;
static constexpr uint32_t GroupSize = 64;
static constexpr uint32_t EmptyWord = 0xFFFFFFFF;

struct FeatureConstants
{
    uint32_t JobBase;
    uint32_t JobCount;
    uint32_t Padding[2];
};

// Mirrors FeatureCellHash in Shaders/Skin.hlsli.
static uint32_t FeatureCellHash(uint32_t chart, uint32_t band, int32_t x, int32_t y)
{
    uint32_t hash = chart * 0x9E3779B1u ^ band * 0x85EBCA77u;
    hash ^= static_cast<uint32_t>(x) * 0xC2B2AE3Du;
    hash = (hash ^ (hash >> 15)) * 0x27D4EB2Fu;
    hash ^= static_cast<uint32_t>(y) * 0x165667B1u;
    hash = (hash ^ (hash >> 13)) * 0x9E3779B1u;
    return hash ^ (hash >> 16);
}

struct FeatureKeyHash
{
    size_t operator()(const FeatureKey& key) const { return FeatureCellHash(key.Chart, key.Band, key.X, key.Y); }
};

struct ResidentCell
{
    uint32_t Slot = 0;
    uint64_t LastUsedFrame = 0;
};

static std::unordered_map<FeatureKey, ResidentCell, FeatureKeyHash> Resident;
static std::unordered_map<FeatureKey, FeatureChart, FeatureKeyHash> Requests;
static std::vector<uint32_t> FreeSlots;
static std::vector<std::pair<FeatureKey, uint32_t>> Jobs;      // Key and slot, generated this frame
static std::vector<FeatureChart> JobCharts;
static BufferHandle Cells = {};
static PipelineLayout* FeatureLayout = nullptr;
static Pipeline* FeaturePipeline = nullptr;
static DescriptorSetHandle FeatureSet = {};
static uint32_t TableBase = 0;
static uint32_t TableMask = 0;
static uint32_t JobBase = 0;
static uint32_t PeakResident = 0;
static uint64_t TotalGenerated = 0;

static void ExecuteFeatures(const PassContext& context)
{
    PipelineExecutor* executor = context.Executor;
    if (Jobs.empty())
    {
        BufferTransition toRead = { Cells, BufferState::Undefined, BufferState::ShaderRead };
        executor->TransitionBuffers({ &toRead, 1 });
        return;
    }

    BufferTransition toWrite = { Cells, BufferState::Undefined, BufferState::ShaderWrite };
    executor->TransitionBuffers({ &toWrite, 1 });
    executor->BindPipeline(FeaturePipeline);
    executor->BindDescriptorSet(FeatureSet);
    FeatureConstants constants = { JobBase, static_cast<uint32_t>(Jobs.size()), { 0, 0 } };
    executor->PushConstants(&constants, sizeof(constants));
    executor->Dispatch((static_cast<uint32_t>(Jobs.size()) + GroupSize - 1) / GroupSize, 1, 1);
    BufferTransition toRead = { Cells, BufferState::ShaderWrite, BufferState::ShaderRead };
    executor->TransitionBuffers({ &toRead, 1 });
}

void FeatureCache::Start(ImageHandle chartMaps)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    BufferDesc desc;
    desc.Size = GetBytes();
    desc.Usage.Storage = true;
    desc.Usage.Access = DeviceLocalMemory;
    Cells = allocator->CreateBuffer(desc);
    FreeSlots.clear();
    for (uint32_t slot = CellCount; slot > 0; slot--)
        FreeSlots.push_back(slot - 1);

    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);
    ResourceLayout resources;
    resources.VisibleStages = computeStage;
    resources.Bindings = {
        { .Type = DescriptorType::ReadWriteStorageBuffer, .Slot = 0, .Set = FeatureSetIndex, .Count = 1, .Stride = 8 },
        { .Type = DescriptorType::StorageBuffer, .Slot = 1, .Set = FeatureSetIndex, .Count = 1, .Stride = 16 },
        { .Type = DescriptorType::SampledImage, .Slot = 2, .Set = FeatureSetIndex, .Count = 1 },
    };
    FeatureLayout = PipelineLayout::Create(resources, { { .Size = sizeof(FeatureConstants), .VisibleStages = computeStage } });
    ShaderStage shader = LoadShader("cs_skin_features");
    FeaturePipeline = Pipeline::CreateCompute(FeatureLayout, shader);
    ReleaseShader(shader);
    FeatureSet = allocator->AllocateDescriptorSet(FeatureLayout->GetID(), FeatureSetIndex, {
        DescriptorSetBinding::ForBuffer(0, Cells),
        DescriptorSetBinding::ForBuffer(1, allocator->GetFrameRing()),
        DescriptorSetBinding::ForImage(2, chartMaps),
    });

    PassDesc pass;
    pass.Name = "Skin features";
    pass.Kind = PassKind::Compute;
    pass.Order = INT_MIN + 2;
    pass.Execute = ExecuteFeatures;
    Renderer::GetPassGraph().AddPass(std::move(pass));
}

void FeatureCache::Shutdown()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    if (FeatureSet.IsValid())
        allocator->FreeDescriptorSet(FeatureSet);
    if (Cells.IsValid())
        allocator->DestroyBuffer(Cells);
    delete FeaturePipeline;
    delete FeatureLayout;
    FeatureSet = {};
    Cells = {};
    FeaturePipeline = nullptr;
    FeatureLayout = nullptr;
    Resident.clear();
    Requests.clear();
    Jobs.clear();
    JobCharts.clear();
}

void FeatureCache::Request(const FeatureKey& key, const FeatureChart& chart)
{
    Requests.try_emplace(key, chart);
}

void FeatureCache::Schedule(uint64_t frame)
{
    Jobs.clear();
    JobCharts.clear();
    std::vector<std::pair<FeatureKey, FeatureChart>> missing;
    for (const auto& [key, chart] : Requests)
    {
        auto found = Resident.find(key);
        if (found != Resident.end())
            found->second.LastUsedFrame = frame;
        else if (missing.size() < JobsPerFrame)
            missing.push_back({ key, chart });
    }

    // Slots come from the free list, then from the least recently used cells no request touched
    // this frame, oldest first.
    std::vector<std::pair<uint64_t, FeatureKey>> evictable;
    if (missing.size() > FreeSlots.size())
    {
        for (const auto& [key, cell] : Resident)
            if (cell.LastUsedFrame < frame)
                evictable.push_back({ cell.LastUsedFrame, key });
        std::sort(evictable.begin(), evictable.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    }
    for (const auto& [key, chart] : missing)
    {
        if (FreeSlots.empty() && !evictable.empty())
        {
            auto victim = Resident.find(evictable.back().second);
            evictable.pop_back();
            FreeSlots.push_back(victim->second.Slot);
            Resident.erase(victim);
        }
        if (FreeSlots.empty())
            break;
        uint32_t slot = FreeSlots.back();
        FreeSlots.pop_back();
        Resident[key] = { slot, frame };
        Jobs.push_back({ key, slot });
        JobCharts.push_back(chart);
    }
    Requests.clear();
    PeakResident = (std::max)(PeakResident, static_cast<uint32_t>(Resident.size()));
    TotalGenerated += Jobs.size();

    // The GPU hash: open addressing, every cell within ProbeSteps of its hash or left out this frame.
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    uint32_t entryCount = std::bit_ceil(CellCount * 2);
    std::vector<XMUINT4> table(entryCount, XMUINT4(EmptyWord, EmptyWord, EmptyWord, EmptyWord));
    for (const auto& [key, cell] : Resident)
    {
        uint32_t index = FeatureCellHash(key.Chart, key.Band, key.X, key.Y);
        for (uint32_t step = 0; step < ProbeSteps; step++)
        {
            XMUINT4& entry = table[(index + step) & (entryCount - 1)];
            if (entry.z != EmptyWord)
                continue;
            entry = { static_cast<uint32_t>(key.X), static_cast<uint32_t>(key.Y), (key.Chart << 8) | key.Band, cell.Slot };
            break;
        }
    }
    FrameAllocation tableAllocation = allocator->AllocateFrameData(table.size() * sizeof(XMUINT4), sizeof(XMUINT4));
    std::memcpy(tableAllocation.Address, table.data(), table.size() * sizeof(XMUINT4));
    TableBase = static_cast<uint32_t>(tableAllocation.Offset / sizeof(XMUINT4));
    TableMask = entryCount - 1;

    std::vector<XMUINT4> jobs((std::max)(Jobs.size(), size_t(1)) * 2, XMUINT4(0, 0, 0, 0));
    for (size_t i = 0; i < Jobs.size(); i++)
    {
        const FeatureKey& key = Jobs[i].first;
        const FeatureChart& chart = JobCharts[i];
        jobs[i * 2] = { static_cast<uint32_t>(key.X), static_cast<uint32_t>(key.Y), (key.Chart << 8) | key.Band, Jobs[i].second };
        jobs[i * 2 + 1] = { chart.TableOffset, std::bit_cast<uint32_t>(chart.Millimetres[0]), std::bit_cast<uint32_t>(chart.Millimetres[1]),
                            (chart.IsPeriodic ? 1u : 0u) | (chart.ChartLayer << 1) };
    }
    FrameAllocation jobAllocation = allocator->AllocateFrameData(jobs.size() * sizeof(XMUINT4), sizeof(XMUINT4));
    std::memcpy(jobAllocation.Address, jobs.data(), jobs.size() * sizeof(XMUINT4));
    JobBase = static_cast<uint32_t>(jobAllocation.Offset / sizeof(XMUINT4));
}

void FeatureCache::ForgetChart(uint32_t chart)
{
    for (auto it = Resident.begin(); it != Resident.end();)
        if (it->first.Chart == chart)
        {
            FreeSlots.push_back(it->second.Slot);
            it = Resident.erase(it);
        }
        else
            ++it;
}

BufferHandle FeatureCache::GetBuffer() { return Cells; }
uint32_t FeatureCache::GetTableBase() { return TableBase; }
uint32_t FeatureCache::GetTableMask() { return TableMask; }
uint64_t FeatureCache::GetBytes() { return static_cast<uint64_t>(CellCount) * RecordEntries * 8; }
uint32_t FeatureCache::GetResidentCells() { return static_cast<uint32_t>(Resident.size()); }
uint32_t FeatureCache::GetPeakResidentCells() { return PeakResident; }
uint32_t FeatureCache::GetCellsGenerated() { return static_cast<uint32_t>(Jobs.size()); }
uint64_t FeatureCache::GetTotalCellsGenerated() { return TotalGenerated; }
