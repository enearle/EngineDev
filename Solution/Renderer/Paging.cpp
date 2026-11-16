#include "Paging.h"

#include <algorithm>
#include <array>
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
static constexpr uint32_t MaxQueuedPages = 64;
static constexpr uint32_t ConstantsSize = 512;

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
    uint32_t Padding;
    DirectX::XMUINT4 Textures[Paging::MaxTextures];     // x: level count
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
    uint32_t LevelCount = 0;
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

static PageSlot MakeSlot(uint32_t slot)
{
    return { slot, slot % Settings.SlotsPerSide * Paging::SlotSize, slot / Settings.SlotsPerSide * Paging::SlotSize, AtlasImages };
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

static void TransitionAtlas(PipelineExecutor* executor, ImageLayout from, ImageLayout to)
{
    std::vector<ImageTransition> transitions;
    for (ImageHandle image : AtlasImages)
        transitions.push_back({ image, from, to });
    executor->TransitionImages(transitions);
}

static void ExecuteFills(const PassContext& context)
{
    if (Fills.empty())
        return;

    TransitionAtlas(context.Executor, ImageLayout::Storage, ImageLayout::TransferDst);
    for (const auto& [page, slot] : Fills)
        if (Textures[page.Texture].IsLive)
            Textures[page.Texture].Producer->Fill(context.Executor, page, MakeSlot(slot));
    TransitionAtlas(context.Executor, ImageLayout::TransferDst, ImageLayout::Storage);
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
    }

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

static PageKey CoarsestMissingAncestor(PageKey page)
{
    PageKey coarsest = page;
    while (page.Level > 0)
    {
        page = Parent(page);
        if (Resident.contains(page))
            break;
        coarsest = page;
    }
    return coarsest;
}

static bool IsLiveRequest(const PageKey& page)
{
    if (page.Texture >= Textures.size() || !Textures[page.Texture].IsLive || page.Level >= Textures[page.Texture].LevelCount)
        return false;
    uint32_t pagesPerSide = 1u << page.Level;
    return page.X < pagesPerSide && page.Y < pagesPerSide;
}

// Turns the latest feedback into queued pages: the coarsest missing ancestor of each missing page,
// coarse levels and the most requested first.
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
        wanted[CoarsestMissingAncestor(page)] += count;
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

    for (const auto& [page, queued] : ready)
    {
        if (Fills.size() >= Settings.FillsPerFrame)
            break;
        if (page.Level > 0 && !Resident.contains(Parent(page)))
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
        Stats.Fills++;
        Stats.TotalFills++;

        auto requested = Requested.find(page);
        if (requested != Requested.end())
        {
            Stats.LongestWait = (std::max)(Stats.LongestWait, static_cast<uint32_t>(frame - requested->second.First));
            Requested.erase(requested);
        }
        if (FillObserver)
            FillObserver(page, MakeSlot(slot));
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
    for (uint32_t i = 0; i < Textures.size(); i++)
        constants.Textures[i].x = Textures[i].IsLive ? Textures[i].LevelCount : 0;

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

PagedTextureID Paging::EnlistTexture(uint32_t levelCount, PageProducer* producer)
{
    if (levelCount == 0 || levelCount > MaxLevelCount)
        throw std::runtime_error("A paged texture has 1 to " + std::to_string(MaxLevelCount) + " levels, not " + std::to_string(levelCount));

    PagedTextureID texture = 0;
    while (texture < Textures.size() && Textures[texture].IsLive)
        texture++;
    if (texture >= MaxTextures)
        throw std::runtime_error("Too many paged textures");
    if (texture == Textures.size())
        Textures.emplace_back();
    Textures[texture] = { levelCount, producer, true };

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

void Paging::SetFillObserver(std::function<void(const PageKey&, const PageSlot&)> observer)
{
    FillObserver = std::move(observer);
}
