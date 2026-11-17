#include "Paging.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <chrono>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include "Readback.h"
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "Window.h"
#include "RHI/BufferAllocator.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"

using namespace RHIStructures;

static constexpr uint32_t TableEntriesPerSlot = 4;
static constexpr uint32_t EmptyWord = 0xFFFFFFFF;
static constexpr uint32_t NoTexture = 0xFF;
static constexpr uint32_t MaxLevelCount = 24;               // Feedback keeps 24 bits of x and y
static constexpr uint32_t MaxFeedbackBlocks = 512 * 512;
static constexpr uint32_t ClearGroupSize = 64;
static constexpr uint32_t ClearSetIndex = 1;
static constexpr uint32_t JitterFrames = Paging::FeedbackBlockSize * Paging::FeedbackBlockSize;
static constexpr uint64_t FramesBeforeCancel = 60;
static constexpr uint32_t MaxQueuedPages = 256;
static constexpr uint32_t ConstantsSize = 256;
static constexpr uint32_t EntriesPerTexture = 2;

// Mirrors the cbuffer PagingConstants in Shaders/Paging.hlsli.
struct PagingConstants
{
    uint32_t TableBase;                 // In 16-byte entries of the frame ring
    uint32_t TableMask;
    uint32_t UserBase;
    uint32_t SlotsPerSide;
    float AtlasTexelSize[2];
    uint32_t FeedbackSize[2];           // In blocks
    uint32_t Jitter[2];
    uint32_t IsTinted;
    uint32_t TexturesBase;              // In 16-byte entries of the frame ring, EntriesPerTexture per texture
    uint32_t LevelBias;
    uint32_t Padding[3];
};
static_assert(sizeof(PagingConstants) <= ConstantsSize);

struct ClearConstants
{
    uint32_t Count;
    uint32_t Padding[3];
};

// Mirrors HashPage in Shaders/Paging.hlsli.
static uint32_t HashPage(uint32_t texture, uint32_t level, uint32_t x, uint32_t y)
{
    uint32_t hash = texture * 0x9E3779B1u ^ level * 0x85EBCA77u;
    hash ^= x * 0xC2B2AE3Du;
    hash = (hash ^ (hash >> 15)) * 0x27D4EB2Fu;
    hash ^= y * 0x165667B1u;
    hash = (hash ^ (hash >> 13)) * 0x9E3779B1u;
    return hash ^ (hash >> 16);
}

struct PageKeyHash
{
    size_t operator()(const PageKey& page) const { return HashPage(page.Texture, page.Level, page.X, page.Y); }
};

struct PagedTexture
{
    PagedTextureDesc Desc;
    PageProducer* Producer = nullptr;
    bool IsLive = false;
};

struct ResidentPage
{
    uint32_t Slot = 0;
    uint64_t LastUsedFrame = 0;
    uint32_t ResidentChildren = 0;
    bool IsPinned = false;
};

struct QueuedPage
{
    uint64_t LastWantedFrame = 0;
    uint32_t Requests = 0;
    bool IsPinned = false;
};

struct RequestTimes
{
    uint64_t First = 0;
    uint64_t Last = 0;
};

static PagingSettings Settings;
static std::vector<PagedTexture> Textures;
static std::unordered_map<PageKey, ResidentPage, PageKeyHash> Resident;
static std::unordered_map<PageKey, QueuedPage, PageKeyHash> Queued;
static std::unordered_map<PageKey, RequestTimes, PageKeyHash> Requested;
static std::vector<bool> IsSlotUsed;
static std::vector<std::array<uint32_t, 2>> SlotUserWords;
static std::vector<std::pair<PageKey, uint32_t>> Fills;    // Page and slot, filled this frame

static std::vector<ImageHandle> AtlasImages;
static std::vector<GraphImageID> AtlasGraphImages;
static uint32_t AtlasSize = 0;
static std::vector<ImageHandle> ScratchImages;
static ImageLayout ScratchLayout = ImageLayout::Undefined;
static BufferHandle FeedbackBuffer = {};
static GraphBufferID FeedbackGraphBuffer = 0;
static DescriptorSetHandle PagingSet = {};
static uint32_t ConstantsOffset = 0;
static PipelineLayout* ClearLayout = nullptr;
static Pipeline* ClearPipeline = nullptr;
static DescriptorSetHandle ClearSet = {};
static uint32_t FeedbackBlocks = 0;

static std::unordered_map<PageKey, uint32_t, PageKeyHash> LatestRequests;
static uint64_t LatestFeedbackFrame = 0;
static bool HasNewFeedback = false;
static uint32_t FeedbackFramesWithoutMissing = 0;
static bool IsTintOn = false;
static uint32_t LevelBias = 0;
static PagingStats Stats;
static std::function<void(const PageKey&, const PageSlot&)> FillObserver;

static PageKey Parent(const PageKey& page)
{
    return { page.Texture, page.Level - 1, page.X / 2, page.Y / 2 };
}

static bool HasLiveTextures()
{
    return std::any_of(Textures.begin(), Textures.end(), [](const PagedTexture& texture) { return texture.IsLive; });
}

static PageSlot MakeSlot(const PageKey& page, uint32_t slot)
{
    PageSlot made = { slot, slot % Settings.SlotsPerSide * Paging::SlotSize, slot / Settings.SlotsPerSide * Paging::SlotSize, AtlasImages, AtlasImages };
    if (page.Level > 0)
    {
        auto parent = Resident.find(Parent(page));
        if (parent != Resident.end())
        {
            made.HasParent = true;
            made.ParentX = parent->second.Slot % Settings.SlotsPerSide * Paging::SlotSize;
            made.ParentY = parent->second.Slot / Settings.SlotsPerSide * Paging::SlotSize;
        }
    }
    return made;
}

static PageSlot MakeScratchSlot(const PageKey& page, uint32_t slot, uint32_t scratchSlot)
{
    PageSlot made = MakeSlot(page, slot);
    made.X = scratchSlot % Settings.ScratchSlotsPerSide * Paging::SlotSize;
    made.Y = scratchSlot / Settings.ScratchSlotsPerSide * Paging::SlotSize;
    made.Images = ScratchImages;
    return made;
}

static bool IsComputingFill(const PageKey& page)
{
    return Textures[page.Texture].IsLive && Textures[page.Texture].Producer->IsComputing();
}

// Feedback holds two words per block: level and x, then texture and y, each with 24 bits of
// coordinate. A cleared block has texture 0xFF.
static void ReceiveFeedback(const ReadbackResult& result)
{
    if (result.RequestedFrame < LatestFeedbackFrame)
        return;

    LatestRequests.clear();
    const uint32_t* words = reinterpret_cast<const uint32_t*>(result.Data);
    for (uint64_t i = 0; i + 1 < result.Size / sizeof(uint32_t); i += 2)
    {
        uint32_t texture = words[i + 1] >> 24;
        if (texture == NoTexture)
            continue;
        PageKey page = { texture, words[i] >> 24, words[i] & 0xFFFFFF, words[i + 1] & 0xFFFFFF };
        LatestRequests[page]++;
    }
    LatestFeedbackFrame = result.RequestedFrame;
    Stats.FeedbackLatency = static_cast<uint32_t>(result.DeliveredFrame - result.RequestedFrame);
    HasNewFeedback = true;
}

static void ExecuteClear(const PassContext& context)
{
    if (!HasLiveTextures() || FeedbackBlocks == 0)
        return;

    PipelineExecutor* executor = context.Executor;
    executor->BindPipeline(ClearPipeline);
    executor->BindDescriptorSet(ClearSet);
    ClearConstants constants = { FeedbackBlocks, { 0, 0, 0 } };
    executor->PushConstants(&constants, sizeof(constants));
    executor->Dispatch((FeedbackBlocks + ClearGroupSize - 1) / ClearGroupSize, 1, 1);
}

static void TransitionAll(PipelineExecutor* executor, std::span<const ImageHandle> images, ImageLayout from, ImageLayout to)
{
    if (from == to)
        return;
    std::vector<ImageTransition> transitions;
    for (ImageHandle image : images)
        transitions.push_back({ image, from, to });
    executor->TransitionImages(transitions);
}

static void SetScratchLayout(PipelineExecutor* executor, ImageLayout layout)
{
    TransitionAll(executor, ScratchImages, ScratchLayout, layout);
    ScratchLayout = layout;
}

// Fills run a level at a time, coarse first, so a child reads what its parent was just given. A
// group holds at most one scratch image's worth of computing fills: they write scratch while the
// atlas stays readable, then scratch is copied into the atlas beside the copying fills.
static void ExecuteFills(const PassContext& context)
{
    if (Fills.empty())
        return;

    PipelineExecutor* executor = context.Executor;
    uint32_t scratchSlots = Settings.ScratchSlotsPerSide * Settings.ScratchSlotsPerSide;
    TransitionAll(executor, AtlasImages, ImageLayout::Storage, ImageLayout::ShaderReadOnly);
    for (size_t first = 0; first < Fills.size();)
    {
        uint32_t level = Fills[first].first.Level;
        size_t end = first;
        uint32_t computing = 0;
        while (end < Fills.size() && Fills[end].first.Level == level)
        {
            bool isComputing = IsComputingFill(Fills[end].first);
            if (isComputing && computing == scratchSlots)
                break;
            computing += isComputing ? 1 : 0;
            end++;
        }

        if (computing > 0)
        {
            SetScratchLayout(executor, ImageLayout::Storage);
            uint32_t scratchSlot = 0;
            for (size_t i = first; i < end; i++)
                if (IsComputingFill(Fills[i].first))
                    Textures[Fills[i].first.Texture].Producer->Fill(executor, Fills[i].first, MakeScratchSlot(Fills[i].first, Fills[i].second, scratchSlot++));
            SetScratchLayout(executor, ImageLayout::TransferSrc);
        }

        TransitionAll(executor, AtlasImages, ImageLayout::ShaderReadOnly, ImageLayout::TransferDst);
        uint32_t scratchSlot = 0;
        for (size_t i = first; i < end; i++)
        {
            const auto& [page, slot] = Fills[i];
            if (!Textures[page.Texture].IsLive)
                continue;
            if (!IsComputingFill(page))
            {
                Textures[page.Texture].Producer->Fill(executor, page, MakeSlot(page, slot));
                continue;
            }
            PageSlot scratch = MakeScratchSlot(page, slot, scratchSlot++);
            PageSlot atlas = MakeSlot(page, slot);
            for (size_t image = 0; image < AtlasImages.size(); image++)
                executor->CopyImage(ScratchImages[image], { 0, 0, scratch.X, scratch.Y, Paging::SlotSize, Paging::SlotSize },
                                    AtlasImages[image], { 0, 0, atlas.X, atlas.Y, Paging::SlotSize, Paging::SlotSize });
        }
        TransitionAll(executor, AtlasImages, ImageLayout::TransferDst, ImageLayout::ShaderReadOnly);
        first = end;
    }
    TransitionAll(executor, AtlasImages, ImageLayout::ShaderReadOnly, ImageLayout::Storage);
}

void Paging::Start(const PagingSettings& settings)
{
    Settings = settings;
    uint32_t slotCount = Settings.SlotsPerSide * Settings.SlotsPerSide;
    AtlasSize = Settings.SlotsPerSide * SlotSize;
    IsSlotUsed.assign(slotCount, false);
    SlotUserWords.assign(slotCount, { 0, 0 });
    Stats = {};
    Stats.Slots = slotCount;

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    PassGraph& graph = Renderer::GetPassGraph();
    for (Format& format : Settings.AtlasFormats)
        if (format == Format::R16G16B16A16_UNORM && !allocator->CanWriteStorageImage(format))
        {
            std::cout << "Paging: R16G16B16A16_UNORM cannot be written from compute here, so the atlas uses R16G16B16A16_FLOAT" << std::endl;
            format = Format::R16G16B16A16_FLOAT;
        }

    uint32_t scratchSize = Settings.ScratchSlotsPerSide * SlotSize;
    for (Format format : Settings.AtlasFormats)
    {
        ImageDesc desc;
        desc.Width = AtlasSize;
        desc.Height = AtlasSize;
        desc.Format = format;
        desc.Usage.Sampled = true;
        desc.Usage.Storage = true;
        desc.Usage.TransferSource = true;
        desc.Usage.TransferDestination = true;
        desc.Access = DeviceLocalMemory;
        ImageHandle image = allocator->CreateImage(desc);
        AtlasImages.push_back(image);
        AtlasGraphImages.push_back(graph.ImportImage(image, ImageLayout::Undefined, ImageLayout::ShaderReadOnly));
        Stats.AtlasBytes += static_cast<uint64_t>(AtlasSize) * AtlasSize * FormatSize(format);

        desc.Width = scratchSize;
        desc.Height = scratchSize;
        desc.Usage.TransferDestination = false;
        ScratchImages.push_back(allocator->CreateImage(desc));
        Stats.ScratchBytes += static_cast<uint64_t>(scratchSize) * scratchSize * FormatSize(format);
    }
    ScratchLayout = ImageLayout::Undefined;

    BufferDesc feedback;
    feedback.Size = static_cast<uint64_t>(MaxFeedbackBlocks) * 2 * sizeof(uint32_t);
    feedback.Usage.Storage = true;
    feedback.Usage.TransferSource = true;
    feedback.Usage.Access = DeviceLocalMemory;
    FeedbackBuffer = allocator->CreateBuffer(feedback);
    FeedbackGraphBuffer = graph.ImportBuffer(FeedbackBuffer);
    Readback::WatchBuffer(FeedbackGraphBuffer);

    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);
    ResourceLayout clearResources;
    clearResources.VisibleStages = computeStage;
    clearResources.Bindings = { { .Type = DescriptorType::ReadWriteStorageBuffer, .Slot = 0, .Set = ClearSetIndex, .Count = 1, .Stride = 8 } };
    ClearLayout = PipelineLayout::Create(clearResources, { { .Size = sizeof(ClearConstants), .VisibleStages = computeStage } });
    ShaderStage shader = LoadShader("cs_paging_clear");
    ClearPipeline = Pipeline::CreateCompute(ClearLayout, shader);
    ReleaseShader(shader);
    ClearSet = allocator->AllocateDescriptorSet(ClearLayout->GetID(), ClearSetIndex, { DescriptorSetBinding::ForBuffer(0, FeedbackBuffer) });

    PassDesc clear;
    clear.Name = "Paging clear";
    clear.Kind = PassKind::Compute;
    clear.Order = INT_MIN + 2;
    clear.BufferWrites = { FeedbackGraphBuffer };
    clear.Execute = ExecuteClear;
    graph.AddPass(std::move(clear));

    PassDesc fill;
    fill.Name = "Paging fill";
    fill.Kind = PassKind::Compute;
    fill.Order = INT_MIN + 3;
    for (uint32_t i = 0; i < AtlasGraphImages.size(); i++)
        fill.StorageWrites.push_back({ AtlasGraphImages[i], i });
    fill.Execute = ExecuteFills;
    graph.AddPass(std::move(fill));

    SurfacePassResources& surfaces = Renderer::GetSurfacePassResources();
    for (GraphImageID image : AtlasGraphImages)
        surfaces.SampledReads.push_back({ image, 0 });
    surfaces.BufferWrites.push_back(FeedbackGraphBuffer);
}

void Paging::Shutdown()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    for (ImageHandle image : AtlasImages)
        allocator->DestroyImage(image);
    for (ImageHandle image : ScratchImages)
        allocator->DestroyImage(image);
    ScratchImages.clear();
    if (FeedbackBuffer.IsValid())
        allocator->DestroyBuffer(FeedbackBuffer);
    if (PagingSet.IsValid())
        allocator->FreeDescriptorSet(PagingSet);
    if (ClearSet.IsValid())
        allocator->FreeDescriptorSet(ClearSet);
    delete ClearPipeline;
    delete ClearLayout;

    AtlasImages.clear();
    AtlasGraphImages.clear();
    FeedbackBuffer = {};
    PagingSet = {};
    ClearSet = {};
    ClearPipeline = nullptr;
    ClearLayout = nullptr;
    Textures.clear();
    Resident.clear();
    Queued.clear();
    Requested.clear();
    LatestRequests.clear();
    Fills.clear();
    FillObserver = nullptr;
}

std::vector<DescriptorBinding> Paging::GetPagingSetBindings()
{
    std::vector<DescriptorBinding> bindings = {
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = PagingSetIndex, .Count = 1, .Size = ConstantsSize },
        { .Type = DescriptorType::StorageBuffer, .Slot = 1, .Set = PagingSetIndex, .Count = 1, .Stride = 16 },
        { .Type = DescriptorType::ReadWriteStorageBuffer, .Slot = 2, .Set = PagingSetIndex, .Count = 1, .Stride = 8 },
    };
    for (uint32_t i = 0; i < Settings.AtlasFormats.size(); i++)
        bindings.push_back({ .Type = DescriptorType::SampledImage, .Slot = 3 + i, .Set = PagingSetIndex, .Count = 1 });
    return bindings;
}

void Paging::BindPagingSet(PipelineExecutor* executor)
{
    if (!PagingSet.IsValid())
    {
        BufferAllocator* allocator = BufferAllocator::GetInstance();
        std::vector<DescriptorSetBinding> bindings = {
            DescriptorSetBinding::ForBuffer(0, allocator->GetFrameRing()),
            DescriptorSetBinding::ForBuffer(1, allocator->GetFrameRing()),
            DescriptorSetBinding::ForBuffer(2, FeedbackBuffer),
        };
        for (uint32_t i = 0; i < AtlasImages.size(); i++)
            bindings.push_back(DescriptorSetBinding::ForImage(3 + i, AtlasImages[i]));
        PagingSet = allocator->AllocateDescriptorSet(Renderer::GetSceneLayout()->GetID(), PagingSetIndex, bindings);
    }
    uint32_t offsets[] = { ConstantsOffset };
    executor->BindDescriptorSet(PagingSet, offsets);
}

static void StampUsed(PageKey page, uint64_t frame)
{
    while (true)
    {
        auto found = Resident.find(page);
        if (found != Resident.end())
            found->second.LastUsedFrame = frame;
        if (page.Level == 0)
            return;
        page = Parent(page);
    }
}

static bool IsLiveRequest(const PageKey& page)
{
    if (page.Texture >= Textures.size() || !Textures[page.Texture].IsLive || page.Level >= Textures[page.Texture].Desc.LevelCount)
        return false;
    uint32_t pagesPerSide = 1u << page.Level;
    return page.X < pagesPerSide && page.Y < pagesPerSide;
}

// Turns the latest feedback into queued pages: each missing page and every missing ancestor of it,
// coarse levels and the most requested first, so the whole chain can be filled in one frame.
static void QueueRequestedPages(uint64_t frame)
{
    std::unordered_map<PageKey, uint32_t, PageKeyHash> wanted;
    uint32_t missing = 0;
    for (const auto& [page, count] : LatestRequests)
    {
        if (!IsLiveRequest(page))
            continue;
        StampUsed(page, frame);
        if (Resident.contains(page))
            continue;

        missing++;
        RequestTimes& times = Requested.try_emplace(page, RequestTimes{ LatestFeedbackFrame, LatestFeedbackFrame }).first->second;
        times.Last = LatestFeedbackFrame;
        for (PageKey ancestor = page;;)
        {
            wanted[ancestor] += count;
            if (ancestor.Level == 0 || Resident.contains(Parent(ancestor)))
                break;
            ancestor = Parent(ancestor);
        }
    }
    Stats.Requests = missing;
    if (HasNewFeedback)
        FeedbackFramesWithoutMissing = missing == 0 ? FeedbackFramesWithoutMissing + 1 : 0;
    HasNewFeedback = false;

    std::vector<std::pair<PageKey, uint32_t>> newPages;
    for (const auto& [page, count] : wanted)
    {
        auto found = Queued.find(page);
        if (found != Queued.end())
        {
            found->second.LastWantedFrame = frame;
            found->second.Requests = count;
        }
        else
            newPages.push_back({ page, count });
    }
    std::sort(newPages.begin(), newPages.end(), [](const auto& a, const auto& b)
    {
        return a.first.Level != b.first.Level ? a.first.Level < b.first.Level : a.second > b.second;
    });
    for (const auto& [page, count] : newPages)
    {
        if (Queued.size() >= MaxQueuedPages)
            break;
        Queued[page] = { frame, count, false };
        Textures[page.Texture].Producer->Begin(page);
    }

    for (auto it = Queued.begin(); it != Queued.end();)
        if (!it->second.IsPinned && frame - it->second.LastWantedFrame > FramesBeforeCancel)
        {
            Textures[it->first.Texture].Producer->Cancel(it->first);
            it = Queued.erase(it);
        }
        else
            ++it;

    std::erase_if(Requested, [](const auto& entry) { return LatestFeedbackFrame - entry.second.Last > JitterFrames; });
}

static void ReleaseSlotOf(const PageKey& page, const ResidentPage& resident)
{
    IsSlotUsed[resident.Slot] = false;
    if (page.Level > 0)
    {
        auto parent = Resident.find(Parent(page));
        if (parent != Resident.end())
            parent->second.ResidentChildren--;
    }
}

// A free slot, or the slot of the least recently used page that is not pinned, has no resident
// child and was not used this frame.
static bool ClaimSlot(uint64_t frame, uint32_t& slot)
{
    for (uint32_t i = 0; i < IsSlotUsed.size(); i++)
        if (!IsSlotUsed[i])
        {
            slot = i;
            return true;
        }

    auto victim = Resident.end();
    for (auto it = Resident.begin(); it != Resident.end(); ++it)
        if (!it->second.IsPinned && it->second.ResidentChildren == 0 && it->second.LastUsedFrame < frame &&
            (victim == Resident.end() || it->second.LastUsedFrame < victim->second.LastUsedFrame))
            victim = it;
    if (victim == Resident.end())
        return false;

    slot = victim->second.Slot;
    ReleaseSlotOf(victim->first, victim->second);
    Resident.erase(victim);
    Stats.Evictions++;
    Stats.TotalEvictions++;
    return true;
}

static void ScheduleFills(uint64_t frame)
{
    Fills.clear();
    std::vector<std::pair<PageKey, QueuedPage>> ready;
    for (const auto& [page, queued] : Queued)
        if (Textures[page.Texture].Producer->IsReady(page))
            ready.push_back({ page, queued });
    std::sort(ready.begin(), ready.end(), [](const auto& a, const auto& b)
    {
        return a.first.Level != b.first.Level ? a.first.Level < b.first.Level : a.second.Requests > b.second.Requests;
    });

    std::unordered_map<PageProducer*, uint32_t> fillsByProducer;
    for (const auto& [page, queued] : ready)
    {
        if (Fills.size() >= Settings.FillsPerFrame)
            break;
        if (page.Level > 0 && !Resident.contains(Parent(page)))
            continue;
        PageProducer* producer = Textures[page.Texture].Producer;
        if (fillsByProducer[producer] >= producer->GetFillsPerFrame())
            continue;
        uint32_t slot = 0;
        if (!ClaimSlot(frame, slot))
            break;

        Queued.erase(page);
        Resident[page] = { slot, frame, 0, queued.IsPinned };
        if (page.Level > 0)
            Resident[Parent(page)].ResidentChildren++;
        IsSlotUsed[slot] = true;
        SlotUserWords[slot] = { 0, 0 };
        Fills.push_back({ page, slot });
        fillsByProducer[producer]++;
        Stats.Fills++;
        Stats.TotalFills++;

        auto requested = Requested.find(page);
        if (requested != Requested.end())
        {
            Stats.LongestWait = (std::max)(Stats.LongestWait, static_cast<uint32_t>(frame - requested->second.First));
            Requested.erase(requested);
        }
        if (FillObserver)
            FillObserver(page, MakeSlot(page, slot));
    }
}

// Coarse pages are placed first, so a page that cannot be placed within ProbeSteps of its hash is
// a fine one, and lookups fall back to its parent until a later frame places it.
static void WriteGpuData(uint64_t frame)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    uint32_t entryCount = 1;
    while (entryCount < IsSlotUsed.size() * TableEntriesPerSlot)
        entryCount <<= 1;

    std::vector<std::pair<PageKey, uint32_t>> pages;
    pages.reserve(Resident.size());
    for (const auto& [page, resident] : Resident)
        pages.push_back({ page, resident.Slot });
    std::sort(pages.begin(), pages.end(), [](const auto& a, const auto& b) { return a.first.Level < b.first.Level; });

    std::vector<DirectX::XMUINT4> table(entryCount, DirectX::XMUINT4(EmptyWord, EmptyWord, EmptyWord, EmptyWord));
    uint32_t unplaced = 0;
    for (const auto& [page, slot] : pages)
    {
        uint32_t index = HashPage(page.Texture, page.Level, page.X, page.Y);
        bool isPlaced = false;
        for (uint32_t step = 0; step < Paging::ProbeSteps && !isPlaced; step++)
        {
            DirectX::XMUINT4& entry = table[(index + step) & (entryCount - 1)];
            if (entry.z != EmptyWord)
                continue;
            entry = { page.X, page.Y, (page.Texture << 8) | page.Level, slot };
            isPlaced = true;
        }
        unplaced += isPlaced ? 0 : 1;
    }
    Stats.UnplacedPages = unplaced;

    FrameAllocation tableAllocation = allocator->AllocateFrameData(entryCount * sizeof(DirectX::XMUINT4), sizeof(DirectX::XMUINT4));
    std::memcpy(tableAllocation.Address, table.data(), entryCount * sizeof(DirectX::XMUINT4));

    std::vector<DirectX::XMUINT4> userWords(SlotUserWords.size());
    for (size_t i = 0; i < SlotUserWords.size(); i++)
        userWords[i] = { SlotUserWords[i][0], SlotUserWords[i][1], 0, 0 };
    FrameAllocation userAllocation = allocator->AllocateFrameData(userWords.size() * sizeof(DirectX::XMUINT4), sizeof(DirectX::XMUINT4));
    std::memcpy(userAllocation.Address, userWords.data(), userWords.size() * sizeof(DirectX::XMUINT4));

    Window* window = Renderer::GetWindow();
    uint32_t blocksX = (window->GetWidth() + Paging::FeedbackBlockSize - 1) / Paging::FeedbackBlockSize;
    uint32_t blocksY = (std::min)((window->GetHeight() + Paging::FeedbackBlockSize - 1) / Paging::FeedbackBlockSize, MaxFeedbackBlocks / (std::max)(blocksX, 1u));
    FeedbackBlocks = HasLiveTextures() ? blocksX * blocksY : 0;
    uint32_t jitter = static_cast<uint32_t>(frame * 37) % JitterFrames;

    PagingConstants constants = {};
    constants.TableBase = static_cast<uint32_t>(tableAllocation.Offset / sizeof(DirectX::XMUINT4));
    constants.TableMask = entryCount - 1;
    constants.UserBase = static_cast<uint32_t>(userAllocation.Offset / sizeof(DirectX::XMUINT4));
    constants.SlotsPerSide = Settings.SlotsPerSide;
    constants.AtlasTexelSize[0] = constants.AtlasTexelSize[1] = 1.0f / AtlasSize;
    constants.FeedbackSize[0] = blocksX;
    constants.FeedbackSize[1] = blocksY;
    constants.Jitter[0] = jitter % Paging::FeedbackBlockSize;
    constants.Jitter[1] = jitter / Paging::FeedbackBlockSize;
    constants.IsTinted = IsTintOn;
    constants.LevelBias = LevelBias;

    std::vector<DirectX::XMUINT4> textureEntries((std::max)(Textures.size(), size_t(1)) * EntriesPerTexture, DirectX::XMUINT4(0, 0, 0, 0));
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i].IsLive)
        {
            const PagedTextureDesc& desc = Textures[i].Desc;
            textureEntries[i * EntriesPerTexture] = { desc.LevelCount, std::bit_cast<uint32_t>(desc.MillimetresU), std::bit_cast<uint32_t>(desc.MillimetresV),
                                                      desc.IsPeriodic ? 1u : 0u };
            textureEntries[i * EntriesPerTexture + 1] = { desc.BandTable, desc.ChartMapLayer, 0, 0 };
        }
    FrameAllocation textureAllocation = allocator->AllocateFrameData(textureEntries.size() * sizeof(DirectX::XMUINT4), sizeof(DirectX::XMUINT4));
    std::memcpy(textureAllocation.Address, textureEntries.data(), textureEntries.size() * sizeof(DirectX::XMUINT4));
    constants.TexturesBase = static_cast<uint32_t>(textureAllocation.Offset / sizeof(DirectX::XMUINT4));

    FrameAllocation constantsAllocation = allocator->AllocateFrameData(ConstantsSize, BufferAllocator::UniformAlignment);
    std::memcpy(constantsAllocation.Address, &constants, sizeof(constants));
    ConstantsOffset = static_cast<uint32_t>(constantsAllocation.Offset);

    if (FeedbackBlocks > 0)
        Readback::RequestBuffer(FeedbackGraphBuffer, 0, static_cast<uint64_t>(FeedbackBlocks) * 2 * sizeof(uint32_t), ReceiveFeedback);
}

void Paging::Update()
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    uint64_t frame = Renderer::GetExecutor()->GetFrameNumber();
    Stats.Fills = 0;
    Stats.Evictions = 0;

    if (HasLiveTextures())
    {
        QueueRequestedPages(frame);
        ScheduleFills(frame);
    }
    else
        Fills.clear();

    WriteGpuData(frame);
    Stats.Textures = static_cast<uint32_t>(std::count_if(Textures.begin(), Textures.end(), [](const PagedTexture& texture) { return texture.IsLive; }));
    Stats.ResidentPages = static_cast<uint32_t>(Resident.size());
    Stats.PeakResidentPages = (std::max)(Stats.PeakResidentPages, Stats.ResidentPages);
    Stats.QueuedPages = static_cast<uint32_t>(Queued.size());
    Stats.UpdateMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

PagedTextureID Paging::EnlistTexture(const PagedTextureDesc& desc, PageProducer* producer)
{
    if (desc.LevelCount == 0 || desc.LevelCount > MaxLevelCount)
        throw std::runtime_error("A paged texture has 1 to " + std::to_string(MaxLevelCount) + " levels, not " + std::to_string(desc.LevelCount));

    PagedTextureID texture = 0;
    while (texture < Textures.size() && Textures[texture].IsLive)
        texture++;
    if (texture >= MaxTextures)
        throw std::runtime_error("Too many paged textures");
    if (texture == Textures.size())
        Textures.emplace_back();
    Textures[texture] = { desc, producer, true };

    PageKey root = { texture, 0, 0, 0 };
    Queued[root] = { Renderer::GetExecutor()->GetFrameNumber(), 0, true };
    producer->Begin(root);
    return texture;
}

void Paging::RetireTexture(PagedTextureID texture)
{
    if (texture >= Textures.size() || !Textures[texture].IsLive)
        return;

    for (auto it = Queued.begin(); it != Queued.end();)
        if (it->first.Texture == texture)
        {
            Textures[texture].Producer->Cancel(it->first);
            it = Queued.erase(it);
        }
        else
            ++it;
    for (auto it = Resident.begin(); it != Resident.end();)
        if (it->first.Texture == texture)
        {
            IsSlotUsed[it->second.Slot] = false;
            it = Resident.erase(it);
        }
        else
            ++it;
    std::erase_if(Requested, [texture](const auto& entry) { return entry.first.Texture == texture; });
    std::erase_if(Fills, [texture](const auto& fill) { return fill.first.Texture == texture; });
    Textures[texture] = {};
}

void Paging::SetTint(bool isTinted)
{
    IsTintOn = isTinted;
}

void Paging::SetLevelBias(uint32_t levels)
{
    LevelBias = levels;
}

void Paging::ResetTexture(PagedTextureID texture)
{
    if (texture >= Textures.size() || !Textures[texture].IsLive)
        return;
    PagedTexture kept = Textures[texture];
    RetireTexture(texture);
    Textures[texture] = kept;
    PageKey root = { texture, 0, 0, 0 };
    Queued[root] = { Renderer::GetExecutor()->GetFrameNumber(), 0, true };
    kept.Producer->Begin(root);
}

void Paging::SetPageUserWords(const PageKey& page, uint32_t first, uint32_t second)
{
    auto found = Resident.find(page);
    if (found != Resident.end())
        SlotUserWords[found->second.Slot] = { first, second };
}

bool Paging::IsResident(const PageKey& page)
{
    return Resident.contains(page);
}

bool Paging::IsViewFullyResident()
{
    return FeedbackFramesWithoutMissing >= JitterFrames;
}

const PagingStats& Paging::GetStats()
{
    return Stats;
}

GraphImageID Paging::GetAtlasImage(uint32_t formatIndex)
{
    return AtlasGraphImages[formatIndex];
}

Format Paging::GetAtlasFormat(uint32_t formatIndex)
{
    return Settings.AtlasFormats[formatIndex];
}

void Paging::SetFillObserver(std::function<void(const PageKey&, const PageSlot&)> observer)
{
    FillObserver = std::move(observer);
}
