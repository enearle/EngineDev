#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "Paging.h"

// A chart of a procedural surface: a paged texture whose pages are baked from a band table. Pages
// are keyed by chart UV, so every copy of a mesh shares them and deformation never invalidates them.
struct SkinChartDesc
{
    float MillimetresU = 0.0f;          // One UV unit along U and V, on the bind pose
    float MillimetresV = 0.0f;
    bool IsPeriodic = false;            // Wraps along U
    std::string BandTable;              // Engine/Materials/<BandTable>.bands
    std::span<const uint8_t> Map;       // MapSize x MapSize R8G8B8A8 chart map (see MeshChart), or empty for a uniform one
    uint32_t LevelCount = 16;
};

struct SkinStats
{
    uint32_t Charts = 0;
    uint32_t PagesBaked = 0;            // This frame
    uint64_t TotalPagesBaked = 0;
    uint64_t ChartMapBytes = 0;
    uint64_t FeatureCacheBytes = 0;
    uint32_t CellsGenerated = 0;        // This frame
    uint32_t ResidentCells = 0;
    uint32_t PeakResidentCells = 0;
    uint64_t TotalCellsGenerated = 0;
};

// Procedural skin: band tables, chart maps and the bake producer, and the skin set (set 4 of the
// scene layout) that procedural surfaces read. One per program, started by the renderer.
//
// A band table is read from a .bands file, one band per line, and checked for changes once a second;
// a change drops every page of the charts that use it. Its GPU copy is written into the frame ring
// each frame, as are the patch tables. Chart maps live in one array image, a layer per chart; layer
// 0 is a uniform field.
class RENDERER_API Skin
{
public:
    static constexpr uint32_t SkinSetIndex = 4;
    static constexpr uint32_t MapSize = 512;
    static constexpr uint32_t MaxChartMaps = 16;
    static constexpr uint32_t MaxBands = 32;
    static constexpr uint32_t MaxBandTables = 16;
    static constexpr uint32_t MaxPatchTables = 16;

    static void Start();
    static void Shutdown();
    // Once per frame, after Paging::Update.
    static void Update();

    static std::vector<RHIStructures::DescriptorBinding> GetSkinSetBindings();
    static void BindSkinSet(PipelineExecutor* executor);

    static PagedTextureID EnlistChart(const SkinChartDesc& desc);
    static void RetireChart(PagedTextureID chart);
    // A mesh's seam patch charts, in the order of its vertices' PatchChart; procedural surfaces name
    // the table by the index returned.
    static uint32_t EnlistPatchTable(std::span<const PagedTextureID> charts);
    static void RetirePatchTable(uint32_t table);
    // Off draws every surface from its primary chart alone, for comparing seams with and without.
    static void SetPatchesEnabled(bool isEnabled);
    static const SkinStats& GetStats();
};
