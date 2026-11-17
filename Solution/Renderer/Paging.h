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

// Where a page is filled. Fill writes the whole slot at X and Y of Images, gutter included: the atlas
// slot for a copying producer, a scratch slot for a computing one. The parent's atlas slot, when
// there is one, is at ParentX and ParentY of AtlasImages.
struct PageSlot
{
    uint32_t Index = 0;                 // The atlas slot the page ends up in
    uint32_t X = 0;
    uint32_t Y = 0;
    std::span<const RHIStructures::ImageHandle> Images;
    std::span<const RHIStructures::ImageHandle> AtlasImages;
    bool HasParent = false;
    uint32_t ParentX = 0;
    uint32_t ParentY = 0;
};

// Makes the texels of pages. Begin may start asynchronous work and IsReady says when it is done.
// Fill runs inside the paging fill pass, a level at a time, coarse first. A copying producer finds
// the atlas images in TransferDst. A computing producer writes a scratch slot with the scratch
// images in Storage and the atlas in ShaderReadOnly, so it can read its parent; the service then
// copies scratch to atlas. The service does every transition. Cancel drops a page that was begun
// and is no longer wanted.
class PageProducer
{
public:
    virtual ~PageProducer() = default;
    virtual bool IsComputing() const { return false; }
    virtual uint32_t GetFillsPerFrame() const { return UINT32_MAX; }
    virtual void Begin(const PageKey& page) = 0;
    virtual bool IsReady(const PageKey& page) = 0;
    virtual void Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot) = 0;
    virtual void Cancel(const PageKey& page) = 0;
};

// A paged texture: its levels and, for one that is a chart of a surface, the chart's size in
// millimetres along U and V, whether it wraps along U, its band table and its chart map layer.
struct PagedTextureDesc
{
    uint32_t LevelCount = 1;
    float MillimetresU = 0.0f;
    float MillimetresV = 0.0f;
    bool IsPeriodic = false;
    uint32_t BandTable = 0;
    uint32_t ChartMapLayer = 0;
};

// Atlas image 0 holds colour or pigment fields (R8G8B8A8); image 1 holds height and slope.
struct PagingSettings
{
    std::vector<RHIStructures::Format> AtlasFormats = { RHIStructures::Format::R8G8B8A8_UNORM, RHIStructures::Format::R16G16B16A16_UNORM };
    uint32_t SlotsPerSide = 16;
    uint32_t ScratchSlotsPerSide = 8;
    uint32_t FillsPerFrame = 64;
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
    uint64_t AtlasBytes = 0;
    uint64_t ScratchBytes = 0;
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
// turns that into fills of every missing ancestor, coarse first, so a whole chain can complete in
// one frame and a page is only ever filled after its parent; level 0 of every texture is pinned. It then writes this frame's page table, a hash, into
// the frame ring, so every frame in flight reads a table that matches the atlas it will see, and
// the per-texture data (PagedTextureDesc) beside it.
// Shaders/Paging.hlsli is the only code that knows the table's form.
class RENDERER_API Paging
{
public:
    static constexpr uint32_t PageSize = 128;
    static constexpr uint32_t Gutter = 4;
    static constexpr uint32_t SlotSize = PageSize + 2 * Gutter;
    static constexpr uint32_t MaxTextures = 64;
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
    static PagedTextureID EnlistTexture(const PagedTextureDesc& desc, PageProducer* producer);
    static void RetireTexture(PagedTextureID texture);

    static void SetTint(bool isTinted);
    static void SetPageUserWords(const PageKey& page, uint32_t first, uint32_t second);
    static bool IsResident(const PageKey& page);
    // True once enough feedback has arrived without a missing page that every pixel was sampled.
    static bool IsViewFullyResident();
    static const PagingStats& GetStats();
    static GraphImageID GetAtlasImage(uint32_t formatIndex);
    static RHIStructures::Format GetAtlasFormat(uint32_t formatIndex);

    // Told of every page given a slot, during Update; the slot is filled later in the same frame.
    static void SetFillObserver(std::function<void(const PageKey&, const PageSlot&)> observer);
};
