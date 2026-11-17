#pragma once
#include <cstdint>
#include <functional>
#include "RHI/RHIStructures.h"

class PipelineExecutor;

// A cell of a point or curve band: the band's lattice cells it covers, by chart, band and position.
// Cells are keyed by lattice, not by page, so a cell stays valid at every deeper zoom.
struct FeatureKey
{
    uint32_t Chart = 0;
    uint32_t Band = 0;
    int32_t X = 0;
    int32_t Y = 0;

    bool operator==(const FeatureKey& other) const = default;
};

// What the feature pass needs to generate a cell of a chart's band.
struct FeatureChart
{
    uint32_t TableOffset = 0;           // The frame's band table, in 16-byte entries of the ring
    float Millimetres[2] = { 0.0f, 0.0f };
    bool IsPeriodic = false;
    uint32_t ChartLayer = 0;
};

// The vector feature cache: a device buffer of CellCount cells, each a count and up to 32
// primitives of 8 bytes. The CPU owns a hash from key to cell, evicts the least recently used cell
// that no request touched this frame, and writes the hash into the frame ring each frame; the
// feature pass fills missing cells before the paging fill pass bakes from them. The buffer is not a
// graph buffer: the feature pass leaves it readable for the bake and the material every frame.
class FeatureCache
{
public:
    static constexpr uint32_t CellCount = 16384;
    static constexpr uint32_t RecordEntries = 33;
    static constexpr uint32_t JobsPerFrame = 2048;
    static constexpr uint32_t ProbeSteps = 4;

    // The chart maps the feature pass reads, for curves that follow the direction field.
    static void Start(RHIStructures::ImageHandle chartMaps);
    static void Shutdown();

    // Any time before Schedule: every cell some page needs. Schedule then generates the missing
    // ones, describing each chart as it is this frame.
    static void Request(const FeatureKey& key);
    static bool IsResident(const FeatureKey& key);
    static void Schedule(uint64_t frame, const std::function<FeatureChart(uint32_t chart)>& describe);
    static void ForgetChart(uint32_t chart);

    static RHIStructures::BufferHandle GetBuffer();
    static uint32_t GetTableBase();
    static uint32_t GetTableMask();
    static uint64_t GetBytes();
    static uint32_t GetResidentCells();
    static uint32_t GetPeakResidentCells();
    static uint32_t GetCellsGenerated();
    static uint64_t GetTotalCellsGenerated();
};
