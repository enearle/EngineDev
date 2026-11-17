#include "Skin.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <DirectXMath.h>
#include "FeatureCache.h"
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "RHI/BufferAllocator.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"

using namespace RHIStructures;
using DirectX::XMFLOAT4;

static constexpr uint32_t BakeSetIndex = 1;
static constexpr uint32_t BakeGroupSize = 8;
static constexpr uint32_t ConstantsSize = 256;
static constexpr uint32_t UploadsPerFrame = 4;
static constexpr uint32_t HeaderEntries = 2;
static constexpr uint32_t BandEntries = 3;
static const std::string BandDirectory = "../Engine/Materials/";

// Mirrors SKIN_FIELD_*, SKIN_PRIMITIVE_* and the profiles in Shaders/Skin.hlsli.
static const char* FieldNames[] = { "height", "melanin", "hemoglobin", "pigment" };
static const char* PrimitiveNames[] = { "noise", "points", "curves" };
static const char* ProfileNames[] = { "dip", "network", "plate", "line" };

// Mirrors BakeConstants in Shaders/cs_skin_bake.hlsl.
struct BakeConstants
{
    uint32_t Level;
    uint32_t Flags;
    uint32_t Page[2];
    uint32_t Scratch[2];
    uint32_t Parent[2];
    float ChartMillimetres[2];
    uint32_t BandTable;
    uint32_t ChartLayer;
    uint32_t Chart;
    uint32_t CellTableBase;
    uint32_t CellTableMask;
    uint32_t Padding;
};

struct Band
{
    std::string Name;
    uint32_t Primitive = 0;
    float Wavelength = 0.0f;
    float Amplitude = 0.0f;
    uint32_t Field = 0;
    uint32_t Seed = 0;
    float Density = 0.0f;
    uint32_t Profile = 0;
    float Width = 0.0f;
    std::string DependsOn;
    float Stretch = 1.0f;
};

struct ParsedTable
{
    std::vector<Band> Bands;            // Coarse to fine by BandScale
    std::vector<XMFLOAT4> Entries;      // The GPU copy: header, then three entries per band
};

struct BandTable
{
    std::string Name;
    std::filesystem::file_time_type WriteTime;
    std::vector<Band> Bands;
    std::vector<XMFLOAT4> Entries;
    uint32_t RingOffset = 0;            // This frame's, in 16-byte entries
};

struct Chart
{
    uint32_t Table = 0;
    uint32_t Layer = 0;
    float Millimetres[2] = { 0.0f, 0.0f };
    bool IsPeriodic = false;
};

struct PendingUpload
{
    uint32_t Layer = 0;
    std::vector<uint8_t> Texels;
};

// A page is ready once every feature cell its bake reads is resident; asking requests the missing
// ones, which the feature pass generates before the next frame's fills.
class SkinBakeProducer : public PageProducer
{
public:
    bool IsComputing() const override { return true; }
    void Begin(const PageKey&) override {}
    bool IsReady(const PageKey& page) override;
    void Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot) override;
    void Cancel(const PageKey&) override {}
};

static std::vector<BandTable> Tables;
static std::unordered_map<PagedTextureID, Chart> Charts;
static std::vector<bool> IsLayerUsed;
static std::vector<PendingUpload> Uploads;
static ImageHandle ChartMaps = {};
static ImageLayout ChartMapsLayout = ImageLayout::Undefined;
static DescriptorSetHandle SkinSet = {};
static uint32_t ConstantsOffset = 0;
static PipelineLayout* BakeLayout = nullptr;
static Pipeline* BakePipeline = nullptr;
static DescriptorSetHandle BakeSet = {};
static SkinBakeProducer Producer;
static SkinStats Stats;
static std::chrono::steady_clock::time_point LastFileCheck;

template <size_t Count>
static uint32_t FindName(const char* const (&names)[Count], const std::string& name, const std::string& what)
{
    for (uint32_t i = 0; i < Count; i++)
        if (name == names[i])
            return i;
    throw std::runtime_error("Unknown " + what + " in a band table: " + name);
}

// Mirrors BandScale and BandWeight in Shaders/Skin.hlsli.
static float BandScale(const Band& band)
{
    return band.Primitive == 0 ? band.Wavelength : band.Width;
}

static float BandWeight(float scale, float footprint)
{
    float t = std::clamp((scale / (std::max)(footprint, 1e-12f) - 2.0f) / 2.0f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float LevelTexelMillimetres(const float millimetres[2], int32_t level)
{
    return (std::max)(millimetres[0], millimetres[1]) / (128.0f * std::ldexp(1.0f, (std::max)(level, 0)));
}

static bool IsDependent(const Band& band)
{
    return !band.DependsOn.empty() && band.DependsOn != "-";
}

// One band per line: name, primitive, wavelength in millimetres, amplitude, field, seed, and for
// points and curves the density, profile, width in millimetres, the band it depends on (- for none)
// and stretch along the direction field. # starts a comment. A dependent band shares its parent's
// lattice, so its wavelength is replaced by the parent's.
static ParsedTable ParseBandTable(const std::string& name)
{
    std::string path = BandDirectory + name + ".bands";
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot read the band table " + path);

    std::vector<Band> bands;
    std::string line;
    for (uint32_t lineNumber = 1; std::getline(file, line); lineNumber++)
    {
        std::istringstream words(line.substr(0, line.find('#')));
        Band band;
        std::string primitive, field;
        if (!(words >> band.Name))
            continue;
        if (!(words >> primitive >> band.Wavelength >> band.Amplitude >> field >> band.Seed))
            throw std::runtime_error(path + "(" + std::to_string(lineNumber) + "): expected name, primitive, wavelength, amplitude, field and seed");
        band.Primitive = FindName(PrimitiveNames, primitive, "primitive");
        band.Field = FindName(FieldNames, field, "field");
        if (band.Primitive != 0)
        {
            std::string profile;
            if (!(words >> band.Density >> profile >> band.Width >> band.DependsOn >> band.Stretch))
                throw std::runtime_error(path + "(" + std::to_string(lineNumber) + "): points and curves also need density, profile, width, depends on and stretch");
            band.Profile = FindName(ProfileNames, profile, "profile");
        }
        if (band.Wavelength <= 0.0f)
            throw std::runtime_error(path + "(" + std::to_string(lineNumber) + "): the wavelength must be positive");
        bands.push_back(band);
    }
    if (bands.size() > Skin::MaxBands)
        throw std::runtime_error(path + " has more than " + std::to_string(Skin::MaxBands) + " bands");
    std::stable_sort(bands.begin(), bands.end(), [](const Band& a, const Band& b) { return BandScale(a) > BandScale(b); });
    for (Band& band : bands)
        for (const Band& other : bands)
            if (IsDependent(band) && other.Name == band.DependsOn)
                band.Wavelength = other.Wavelength;

    // Slope of a height band, root mean square: about amplitude / wavelength for noise, and
    // amplitude / width over the fraction of the surface a point profile covers.
    std::vector<float> variances(bands.size(), 0.0f);
    float heightRange = 1e-6f, slopeRange = 1e-6f, totalVariance = 0.0f;
    float pigmentRanges[3] = { 1e-3f, 1e-3f, 1e-3f };
    for (size_t b = 0; b < bands.size(); b++)
        if (bands[b].Field == 0)
        {
            heightRange += std::abs(bands[b].Amplitude);
            float steepness = bands[b].Primitive == 0 ? bands[b].Wavelength : bands[b].Width;
            float coverage = bands[b].Primitive == 0 ? 1.0f : (std::min)(1.0f, 2.0f * bands[b].Width / bands[b].Wavelength);
            slopeRange += 2.5f * std::abs(bands[b].Amplitude) / steepness;
            variances[b] = coverage * (bands[b].Amplitude / steepness) * (bands[b].Amplitude / steepness);
            totalVariance += variances[b];
        }
        else
            pigmentRanges[bands[b].Field - 1] += std::abs(bands[b].Amplitude);

    std::vector<XMFLOAT4> entries;
    entries.push_back({ static_cast<float>(bands.size()), heightRange, slopeRange, (std::max)({ pigmentRanges[0], pigmentRanges[1], pigmentRanges[2] }) });
    entries.push_back({ totalVariance, 0.0f, 0.0f, 0.0f });
    float finerVariance = totalVariance;
    for (size_t b = 0; b < bands.size(); b++)
    {
        const Band& band = bands[b];
        int32_t dependsOn = -1;
        for (size_t other = 0; other < bands.size(); other++)
            if (bands[other].Name == band.DependsOn)
                dependsOn = static_cast<int32_t>(other);
        finerVariance -= variances[b];
        entries.push_back({ band.Wavelength, band.Amplitude, static_cast<float>(band.Field), static_cast<float>(band.Primitive) });
        entries.push_back({ std::bit_cast<float>(band.Seed), band.Density, band.Width, band.Stretch });
        entries.push_back({ static_cast<float>(band.Profile), static_cast<float>(dependsOn), variances[b], (std::max)(finerVariance, 0.0f) });
    }
    return { bands, entries };
}

static uint32_t LoadBandTable(const std::string& name)
{
    for (uint32_t i = 0; i < Tables.size(); i++)
        if (Tables[i].Name == name)
            return i;
    if (Tables.size() >= Skin::MaxBandTables)
        throw std::runtime_error("Too many band tables");
    BandTable table;
    table.Name = name;
    table.WriteTime = std::filesystem::last_write_time(BandDirectory + name + ".bands");
    ParsedTable parsed = ParseBandTable(name);
    table.Bands = std::move(parsed.Bands);
    table.Entries = std::move(parsed.Entries);
    Tables.push_back(std::move(table));
    return static_cast<uint32_t>(Tables.size() - 1);
}

// A changed table is reparsed and every chart that uses it starts again from level 0. A table that
// fails to parse keeps its last good version.
static void ReloadChangedTables()
{
    for (uint32_t i = 0; i < Tables.size(); i++)
    {
        std::string path = BandDirectory + Tables[i].Name + ".bands";
        std::error_code error;
        std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
        if (error || writeTime == Tables[i].WriteTime)
            continue;
        Tables[i].WriteTime = writeTime;
        try
        {
            ParsedTable parsed = ParseBandTable(Tables[i].Name);
            Tables[i].Bands = std::move(parsed.Bands);
            Tables[i].Entries = std::move(parsed.Entries);
        }
        catch (const std::exception& failure)
        {
            std::cout << "Skin: " << failure.what() << "; keeping the previous table" << std::endl;
            continue;
        }
        std::cout << "Skin: reloaded " << path << std::endl;
        for (const auto& [chart, record] : Charts)
            if (record.Table == i)
            {
                FeatureCache::ForgetChart(chart);
                Paging::ResetTexture(chart);
            }
    }
}

void SkinBakeProducer::Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot)
{
    auto found = Charts.find(page.Texture);
    if (found == Charts.end())
        return;
    const Chart& chart = found->second;

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    if (!BakeSet.IsValid())
        BakeSet = allocator->AllocateDescriptorSet(BakeLayout->GetID(), BakeSetIndex, {
            DescriptorSetBinding::ForImage(0, slot.AtlasImages[0]),
            DescriptorSetBinding::ForImage(1, slot.AtlasImages[1]),
            DescriptorSetBinding::ForImage(2, slot.Images[0], SubresourceRange::Mip(0)),
            DescriptorSetBinding::ForImage(3, slot.Images[1], SubresourceRange::Mip(0)),
            DescriptorSetBinding::ForBuffer(4, allocator->GetFrameRing()),
            DescriptorSetBinding::ForImage(5, ChartMaps),
            DescriptorSetBinding::ForBuffer(6, FeatureCache::GetBuffer()),
        });

    BakeConstants constants = {};
    constants.Level = page.Level;
    constants.Flags = (slot.HasParent ? 1u : 0u) | (chart.IsPeriodic ? 2u : 0u);
    constants.Page[0] = page.X;
    constants.Page[1] = page.Y;
    constants.Scratch[0] = slot.X;
    constants.Scratch[1] = slot.Y;
    constants.Parent[0] = slot.ParentX;
    constants.Parent[1] = slot.ParentY;
    constants.ChartMillimetres[0] = chart.Millimetres[0];
    constants.ChartMillimetres[1] = chart.Millimetres[1];
    constants.BandTable = Tables[chart.Table].RingOffset;
    constants.ChartLayer = chart.Layer;
    constants.Chart = page.Texture;
    constants.CellTableBase = FeatureCache::GetTableBase();
    constants.CellTableMask = FeatureCache::GetTableMask();

    executor->BindPipeline(BakePipeline);
    executor->BindDescriptorSet(BakeSet);
    executor->PushConstants(&constants, sizeof(constants));
    uint32_t groups = (Paging::SlotSize + BakeGroupSize - 1) / BakeGroupSize;
    executor->Dispatch(groups, groups, 1);
    Stats.PagesBaked++;
    Stats.TotalPagesBaked++;
}

static void ExecuteUploads(const PassContext& context)
{
    if (Uploads.empty())
        return;

    PipelineExecutor* executor = context.Executor;
    ImageTransition toCopy = { ChartMaps, ChartMapsLayout, ImageLayout::TransferDst };
    executor->TransitionImages({ &toCopy, 1 });

    uint32_t rowPitch = CopyRowPitch(Format::R8G8B8A8_UNORM, Skin::MapSize);
    size_t count = (std::min)(Uploads.size(), static_cast<size_t>(UploadsPerFrame));
    for (size_t i = 0; i < count; i++)
    {
        FrameAllocation staging = BufferAllocator::GetInstance()->AllocateFrameData(static_cast<uint64_t>(rowPitch) * Skin::MapSize, CopyPlacementAlignment);
        for (uint32_t row = 0; row < Skin::MapSize; row++)
            std::memcpy(static_cast<uint8_t*>(staging.Address) + static_cast<size_t>(row) * rowPitch,
                        Uploads[i].Texels.data() + static_cast<size_t>(row) * Skin::MapSize * 4, Skin::MapSize * 4);
        executor->CopyBufferToImage(staging.Buffer, staging.Offset, rowPitch, ChartMaps, { 0, Uploads[i].Layer, 0, 0, Skin::MapSize, Skin::MapSize });
    }
    Uploads.erase(Uploads.begin(), Uploads.begin() + count);

    ImageTransition toRead = { ChartMaps, ImageLayout::TransferDst, ImageLayout::ShaderReadOnly };
    executor->TransitionImages({ &toRead, 1 });
    ChartMapsLayout = ImageLayout::ShaderReadOnly;
}

void Skin::Start()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    ImageDesc maps;
    maps.Width = MapSize;
    maps.Height = MapSize;
    maps.ArrayLayers = MaxChartMaps;
    maps.Format = Format::R8G8B8A8_UNORM;
    maps.Usage.Sampled = true;
    maps.Usage.TransferDestination = true;
    maps.Access = DeviceLocalMemory;
    ChartMaps = allocator->CreateImage(maps);
    ChartMapsLayout = ImageLayout::Undefined;
    IsLayerUsed.assign(MaxChartMaps, false);
    IsLayerUsed[0] = true;
    Stats = {};
    Stats.ChartMapBytes = static_cast<uint64_t>(MapSize) * MapSize * 4 * MaxChartMaps;

    // Layer 0: direction along U (doubled angle 0), scale 1, covered.
    PendingUpload uniform;
    uniform.Layer = 0;
    uniform.Texels.resize(static_cast<size_t>(MapSize) * MapSize * 4);
    for (size_t i = 0; i < uniform.Texels.size(); i += 4)
    {
        uniform.Texels[i] = 255;
        uniform.Texels[i + 1] = 128;
        uniform.Texels[i + 2] = 128;
        uniform.Texels[i + 3] = 255;
    }
    Uploads.push_back(std::move(uniform));

    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);
    ResourceLayout bakeResources;
    bakeResources.VisibleStages = computeStage;
    bakeResources.Bindings = {
        { .Type = DescriptorType::SampledImage, .Slot = 0, .Set = BakeSetIndex, .Count = 1 },
        { .Type = DescriptorType::SampledImage, .Slot = 1, .Set = BakeSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageImage, .Slot = 2, .Set = BakeSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageImage, .Slot = 3, .Set = BakeSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageBuffer, .Slot = 4, .Set = BakeSetIndex, .Count = 1, .Stride = 16 },
        { .Type = DescriptorType::SampledImage, .Slot = 5, .Set = BakeSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageBuffer, .Slot = 6, .Set = BakeSetIndex, .Count = 1, .Stride = 8 },
    };
    BakeLayout = PipelineLayout::Create(bakeResources, { { .Size = sizeof(BakeConstants), .VisibleStages = computeStage } });
    ShaderStage shader = LoadShader(Paging::GetAtlasFormat(1) == Format::R16G16B16A16_FLOAT ? "cs_skin_bake_float" : "cs_skin_bake");
    BakePipeline = Pipeline::CreateCompute(BakeLayout, shader);
    ReleaseShader(shader);

    PassDesc uploads;
    uploads.Name = "Skin uploads";
    uploads.Kind = PassKind::Compute;
    uploads.Order = INT_MIN + 2;
    uploads.Execute = ExecuteUploads;
    Renderer::GetPassGraph().AddPass(std::move(uploads));
    FeatureCache::Start(ChartMaps);
    Stats.FeatureCacheBytes = FeatureCache::GetBytes();
    LastFileCheck = std::chrono::steady_clock::now();
}

void Skin::Shutdown()
{
    for (const auto& [chart, record] : Charts)
        Paging::RetireTexture(chart);
    Charts.clear();
    Tables.clear();
    Uploads.clear();

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    if (SkinSet.IsValid())
        allocator->FreeDescriptorSet(SkinSet);
    if (BakeSet.IsValid())
        allocator->FreeDescriptorSet(BakeSet);
    if (ChartMaps.IsValid())
        allocator->DestroyImage(ChartMaps);
    delete BakePipeline;
    delete BakeLayout;
    FeatureCache::Shutdown();
    SkinSet = {};
    BakeSet = {};
    ChartMaps = {};
    BakePipeline = nullptr;
    BakeLayout = nullptr;
}

// The cells of a point band a page reads, gutter included: one per lattice cell. True when every one
// is resident.
static bool RequestPageCells(PagedTextureID id, const Chart& chart, uint32_t bandIndex, const PageKey& page)
{
    const Band& band = Tables[chart.Table].Bands[bandIndex];
    double pages = std::ldexp(1.0, static_cast<int>(page.Level));
    double gutter = static_cast<double>(Paging::Gutter) / Paging::PageSize;
    int64_t range[2][2];
    for (int axis = 0; axis < 2; axis++)
    {
        double cells = (std::max)(1.0f, std::nearbyint(chart.Millimetres[axis] / band.Wavelength));
        double position = axis == 0 ? page.X : page.Y;
        range[axis][0] = static_cast<int64_t>(std::floor((position - gutter) * cells / pages));
        range[axis][1] = static_cast<int64_t>(std::floor((position + 1.0 + gutter) * cells / pages));
    }
    if ((range[0][1] - range[0][0] + 1) * (range[1][1] - range[1][0] + 1) > 4096)
        return true;

    bool isResident = true;
    for (int64_t y = range[1][0]; y <= range[1][1]; y++)
        for (int64_t x = range[0][0]; x <= range[0][1]; x++)
        {
            FeatureKey key = { id, bandIndex, static_cast<int32_t>(x), static_cast<int32_t>(y) };
            FeatureCache::Request(key);
            isResident &= FeatureCache::IsResident(key);
        }
    return isResident;
}

static bool IsBakeGrowing(const Band& band, const Chart& chart, uint32_t level)
{
    float scale = BandScale(band);
    return BandWeight(scale, LevelTexelMillimetres(chart.Millimetres, level)) -
           (level > 0 ? BandWeight(scale, LevelTexelMillimetres(chart.Millimetres, level - 1)) : 0.0f) > 0.0f;
}

bool SkinBakeProducer::IsReady(const PageKey& page)
{
    auto found = Charts.find(page.Texture);
    if (found == Charts.end())
        return true;
    const Chart& chart = found->second;
    const std::vector<Band>& bands = Tables[chart.Table].Bands;
    bool isReady = true;
    for (uint32_t b = 0; b < bands.size(); b++)
        if (bands[b].Primitive != 0 && IsBakeGrowing(bands[b], chart, page.Level))
            isReady &= RequestPageCells(page.Texture, chart, b, page);
    return isReady;
}

// Cells come from pages: those under each page baked this frame, for the point bands its bake adds,
// and those under each wanted page, for the point bands that can be live there, a lookup up to two
// levels coarser included.
static void RequestFeatureCells()
{
    Paging::ForEachFill([](const PageKey& page)
    {
        auto found = Charts.find(page.Texture);
        if (found == Charts.end())
            return;
        const Chart& chart = found->second;
        const std::vector<Band>& bands = Tables[chart.Table].Bands;
        for (uint32_t b = 0; b < bands.size(); b++)
        {
            if (bands[b].Primitive == 0)
                continue;
            if (bands[b].Primitive != 0 && IsBakeGrowing(bands[b], chart, page.Level))
                RequestPageCells(page.Texture, chart, b, page);
        }
    });
    Paging::ForEachWantedPage([](const PageKey& page)
    {
        auto found = Charts.find(page.Texture);
        if (found == Charts.end())
            return;
        const Chart& chart = found->second;
        const std::vector<Band>& bands = Tables[chart.Table].Bands;
        for (uint32_t b = 0; b < bands.size(); b++)
        {
            if (bands[b].Primitive == 0)
                continue;
            float scale = BandScale(bands[b]);
            bool isVisible = BandWeight(scale, 0.5f * LevelTexelMillimetres(chart.Millimetres, page.Level)) > 0.0f;
            bool isBakedCoarser = BandWeight(scale, LevelTexelMillimetres(chart.Millimetres, static_cast<int32_t>(page.Level) - 2)) >= 1.0f;
            if (isVisible && !isBakedCoarser)
                RequestPageCells(page.Texture, chart, b, page);
        }
    });
}

void Skin::Update()
{
    Stats.PagesBaked = 0;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - LastFileCheck >= std::chrono::seconds(1))
    {
        LastFileCheck = now;
        ReloadChangedTables();
    }

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    std::vector<XMFLOAT4> entries;
    uint32_t offsets[MaxBandTables] = {};
    FrameAllocation ring = allocator->AllocateFrameData((std::max)(size_t(1), [&]
    {
        size_t total = 0;
        for (const BandTable& table : Tables)
            total += table.Entries.size();
        return total;
    }()) * sizeof(XMFLOAT4), sizeof(XMFLOAT4));
    uint32_t base = static_cast<uint32_t>(ring.Offset / sizeof(XMFLOAT4));
    for (size_t i = 0; i < Tables.size(); i++)
    {
        Tables[i].RingOffset = base + static_cast<uint32_t>(entries.size());
        offsets[i] = Tables[i].RingOffset;
        entries.insert(entries.end(), Tables[i].Entries.begin(), Tables[i].Entries.end());
    }
    if (!entries.empty())
        std::memcpy(ring.Address, entries.data(), entries.size() * sizeof(XMFLOAT4));

    RequestFeatureCells();
    FeatureCache::Schedule(Renderer::GetExecutor()->GetFrameNumber(), [](uint32_t id)
    {
        const Chart& chart = Charts.at(id);
        FeatureChart described;
        described.TableOffset = Tables[chart.Table].RingOffset;
        described.Millimetres[0] = chart.Millimetres[0];
        described.Millimetres[1] = chart.Millimetres[1];
        described.IsPeriodic = chart.IsPeriodic;
        described.ChartLayer = chart.Layer;
        return described;
    });

    FrameAllocation constants = allocator->AllocateFrameData(ConstantsSize, BufferAllocator::UniformAlignment);
    std::memcpy(constants.Address, offsets, sizeof(offsets));
    uint32_t cellTable[2] = { FeatureCache::GetTableBase(), FeatureCache::GetTableMask() };
    std::memcpy(static_cast<uint8_t*>(constants.Address) + sizeof(offsets), cellTable, sizeof(cellTable));
    ConstantsOffset = static_cast<uint32_t>(constants.Offset);
    Stats.Charts = static_cast<uint32_t>(Charts.size());
    Stats.CellsGenerated = FeatureCache::GetCellsGenerated();
    Stats.ResidentCells = FeatureCache::GetResidentCells();
    Stats.PeakResidentCells = FeatureCache::GetPeakResidentCells();
    Stats.TotalCellsGenerated = FeatureCache::GetTotalCellsGenerated();
}

std::vector<DescriptorBinding> Skin::GetSkinSetBindings()
{
    return {
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = SkinSetIndex, .Count = 1, .Size = ConstantsSize },
        { .Type = DescriptorType::StorageBuffer, .Slot = 1, .Set = SkinSetIndex, .Count = 1, .Stride = 16 },
        { .Type = DescriptorType::SampledImage, .Slot = 2, .Set = SkinSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageBuffer, .Slot = 3, .Set = SkinSetIndex, .Count = 1, .Stride = 8 },
    };
}

void Skin::BindSkinSet(PipelineExecutor* executor)
{
    if (!SkinSet.IsValid())
    {
        BufferAllocator* allocator = BufferAllocator::GetInstance();
        SkinSet = allocator->AllocateDescriptorSet(Renderer::GetSceneLayout()->GetID(), SkinSetIndex, {
            DescriptorSetBinding::ForBuffer(0, allocator->GetFrameRing()),
            DescriptorSetBinding::ForBuffer(1, allocator->GetFrameRing()),
            DescriptorSetBinding::ForImage(2, ChartMaps),
            DescriptorSetBinding::ForBuffer(3, FeatureCache::GetBuffer()),
        });
    }
    uint32_t offsets[] = { ConstantsOffset };
    executor->BindDescriptorSet(SkinSet, offsets);
}

PagedTextureID Skin::EnlistChart(const SkinChartDesc& desc)
{
    if (desc.MillimetresU <= 0.0f || desc.MillimetresV <= 0.0f)
        throw std::runtime_error("A skin chart needs a size in millimetres");

    Chart chart;
    chart.Table = LoadBandTable(desc.BandTable);
    chart.Millimetres[0] = desc.MillimetresU;
    chart.Millimetres[1] = desc.MillimetresV;
    chart.IsPeriodic = desc.IsPeriodic;
    if (!desc.Map.empty())
    {
        if (desc.Map.size() != static_cast<size_t>(MapSize) * MapSize * 4)
            throw std::runtime_error("A chart map is " + std::to_string(MapSize) + " x " + std::to_string(MapSize) + " R8G8B8A8");
        auto free = std::find(IsLayerUsed.begin(), IsLayerUsed.end(), false);
        if (free == IsLayerUsed.end())
            throw std::runtime_error("No chart map layer is free");
        *free = true;
        chart.Layer = static_cast<uint32_t>(free - IsLayerUsed.begin());
        Uploads.push_back({ chart.Layer, std::vector<uint8_t>(desc.Map.begin(), desc.Map.end()) });
    }

    PagedTextureDesc texture;
    texture.LevelCount = desc.LevelCount;
    texture.MillimetresU = desc.MillimetresU;
    texture.MillimetresV = desc.MillimetresV;
    texture.IsPeriodic = desc.IsPeriodic;
    texture.BandTable = chart.Table;
    texture.ChartMapLayer = chart.Layer;
    PagedTextureID id = Paging::EnlistTexture(texture, &Producer);
    Charts[id] = chart;
    return id;
}

void Skin::RetireChart(PagedTextureID chart)
{
    auto found = Charts.find(chart);
    if (found == Charts.end())
        return;
    if (found->second.Layer != 0)
        IsLayerUsed[found->second.Layer] = false;
    Charts.erase(found);
    FeatureCache::ForgetChart(chart);
    Paging::RetireTexture(chart);
}

const SkinStats& Skin::GetStats()
{
    return Stats;
}
