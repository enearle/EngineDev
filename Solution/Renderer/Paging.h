#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "PassGraph.h"

class PipelineExecutor;

using PagedTextureID = uint32_t;

// One page of a paged texture. Level 0 is one page covering the whole texture; level L has 2^L
// pages a side.
struct PageKey
{
    PagedTextureID Texture = 0;
    uint32_t Level = 0;
    uint32_t X = 0;
    uint32_t Y = 0;

    bool operator==(const PageKey& other) const = default;
};

// Where a page is filled: its slot in the atlas, as the top-left texel of the slot with its gutter,
// and the atlas images, one per atlas format.
struct PageSlot
{
    uint32_t Index = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    std::span<const RHIStructures::ImageHandle> Images;
};

// Makes the texels of pages. Begin may start asynchronous work and IsReady says when it is done.
// Fill runs inside the paging fill pass with every atlas image in TransferDst and writes the whole
// slot, gutter included, by copying texels or dispatching compute; a producer that dispatches
// compute transitions the images itself and returns them to TransferDst. Cancel drops a page that
// was begun and is no longer wanted.
class PageProducer
{
public:
    virtual ~PageProducer() = default;
    virtual void Begin(const PageKey& page) = 0;
    virtual bool IsReady(const PageKey& page) = 0;
    virtual void Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot) = 0;
    virtual void Cancel(const PageKey& page) = 0;
};

struct PagingSettings
{
    std::vector<RHIStructures::Format> AtlasFormats = { RHIStructures::Format::R8G8B8A8_UNORM };
    uint32_t SlotsPerSide = 16;
    uint32_t FillsPerFrame = 16;
};

struct PagingStats
{
    uint32_t Textures = 0;
    uint32_t ResidentPages = 0;
    uint32_t Slots = 0;
    uint32_t Requests = 0;              // Missing pages asked for by the latest feedback
    uint32_t Fills = 0;                 // This frame
    uint32_t Evictions = 0;             // This frame
    uint32_t QueuedPages = 0;           // Begun and not yet filled
    uint32_t UnplacedPages = 0;         // Resident but left out of this frame's GPU table
    uint32_t FeedbackLatency = 0;       // Frames from the feedback's frame to its delivery
    double UpdateMilliseconds = 0.0;    // CPU time of this frame's Update
    uint64_t TotalFills = 0;
    uint64_t TotalEvictions = 0;
    uint32_t PeakResidentPages = 0;
    uint32_t LongestWait = 0;           // Frames from a page's first request to its fill
};

// Paged textures: quadtrees of 128 x 128 pages, filled on demand into one fixed atlas. One per
// program, started by the renderer.
//
// The G-buffer pixel shader of a paged surface writes the page it wants for one jittered pixel of
// every 8 x 8 block into a feedback buffer, which is read back a few frames later. Each frame the CPU
// turns that into fills, coarsest missing ancestor first, so a page is only ever filled after its
// parent; level 0 of every texture is pinned. It then writes this frame's page table, a hash, into
// the frame ring, so every frame in flight reads a table that matches the atlas it will see.
// Shaders/Paging.hlsli is the only code that knows the table's form.
class RENDERER_API Paging
{
public:
    static constexpr uint32_t PageSize = 128;
    static constexpr uint32_t Gutter = 4;
    static constexpr uint32_t SlotSize = PageSize + 2 * Gutter;
    static constexpr uint32_t MaxTextures = 16;
    static constexpr uint32_t PagingSetIndex = 3;
    static constexpr uint32_t ProbeSteps = 4;
    static constexpr uint32_t FeedbackBlockSize = 8;

    static void Start(const PagingSettings& settings = {});
    static void Shutdown();

    // The paging set: constants, the page table, the feedback buffer and the atlas images.
    static std::vector<RHIStructures::DescriptorBinding> GetPagingSetBindings();
    static void BindPagingSet(PipelineExecutor* executor);

    // Once per frame, after readbacks are delivered and before the pass graph executes.
    static void Update();

    // The producer is not owned and must outlive the texture.
    static PagedTextureID EnlistTexture(uint32_t levelCount, PageProducer* producer);
    static void RetireTexture(PagedTextureID texture);

    static void SetTint(bool isTinted);
    static void SetPageUserWords(const PageKey& page, uint32_t first, uint32_t second);
    static bool IsResident(const PageKey& page);
    // True once enough feedback has arrived without a missing page that every pixel was sampled.
    static bool IsViewFullyResident();
    static const PagingStats& GetStats();
    static GraphImageID GetAtlasImage(uint32_t formatIndex);

    // Told of every page given a slot, during Update; the slot is filled later in the same frame.
    static void SetFillObserver(std::function<void(const PageKey&, const PageSlot&)> observer);
};
