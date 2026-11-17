#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "SkinTest.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "Camera.h"
#include "MaterialRegistry.h"
#include "Paging.h"
#include "Readback.h"
#include "Renderer.h"
#include "Window.h"
#include "Geometry/GeometryImport.h"
#include "Passes/DeferredPasses.h"
#include "Resources/SceneComponent.h"
#include "Scene/SceneNode.h"
#include "Skin/Skin.h"
#include "RHI/PipelineExecutor.h"

using namespace DirectX;

static constexpr float PlaneSize = 60.0f;
static constexpr float PlaneChartMillimetres = 1000.0f;
static constexpr uint32_t MinimumSettleFrames = 8;
static constexpr uint32_t MaximumSettleFrames = 900;
static constexpr uint32_t MeasuredFrames = 60;
static constexpr uint32_t BiasLevels = 2;
static constexpr uint32_t BiasFrames = 4;
static constexpr double AlbedoMeanTolerance = 2.0;          // 8-bit levels
static constexpr double AlbedoPercentileTolerance = 16.0;
static constexpr double NormalMeanTolerance = 0.01;         // Octahedral units, 0 to 1: about 2 degrees
static constexpr double NormalPercentileTolerance = 0.05;   // About 9 degrees
static constexpr double FragmentCostSpread = 0.25;
static const char* CacheDirectory = "../../Cache";

struct Stop
{
    std::string Name;
    XMFLOAT3 Centre;
    float Width;                    // Of the view in world units; 0 for the whole chart
    bool IsSkinFilling;
};

enum class Phase
{
    Settling,
    Measuring,
    CapturingWanted,
    Biasing,
    CapturingCoarse,
    Finished
};

struct Capture
{
    std::vector<uint8_t> Albedo;
    std::vector<uint8_t> Normal;
    bool HasAlbedo = false;
    bool HasNormal = false;
};

struct StopResult
{
    std::string Name;
    bool IsConverged = false;
    uint32_t Convergence = 0;
    double GBufferMilliseconds = 0.0;
    double FrameMilliseconds = 0.0;
    uint32_t PagesBaked = 0;
    double AlbedoMean = 0.0;
    double AlbedoPercentile = 0.0;
    double NormalMean = 0.0;
    double NormalPercentile = 0.0;
    uint64_t MemoryBytes = 0;
    bool IsSkinFilling = false;
};

static std::vector<Stop> Stops;
static std::vector<StopResult> Results;
static size_t StopIndex = 0;
static Phase CurrentPhase = Phase::Finished;
static uint32_t PhaseFrames = 0;
static uint64_t ArrivalFrame = 0;
static uint64_t LastMissingFrame = 0;
static double GBufferSum = 0.0;
static double FrameSum = 0.0;
static uint32_t PagesBakedSum = 0;
static Capture Wanted;
static Capture Coarse;
static uint32_t CaptureWidth = 0;
static uint32_t CaptureHeight = 0;
static bool IsRunning = false;
static bool IsPlane = false;
static PagedTextureID Chart = 0;
static MaterialHandle SkinMaterial = {};
static int HeldStop = -1;

static double GpuMilliseconds(const char* pass)
{
    for (const GpuPassTime& time : Renderer::GetFrameStats().GpuPasses)
        if (time.Name == pass)
            return time.Milliseconds;
    return 0.0;
}

static uint64_t CacheBytes()
{
    const PagingStats& paging = Paging::GetStats();
    const SkinStats& skin = Skin::GetStats();
    return paging.AtlasBytes + paging.ScratchBytes + skin.ChartMapBytes + skin.FeatureCacheBytes;
}

static void CreatePlane()
{
    float half = PlaneSize * 0.5f;
    VertexCache* cache = new VertexCache();
    XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    XMFLOAT3 tangent = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 bitangent = { 0.0f, 0.0f, -1.0f };
    cache->Vertices = {
        { { -half, 0.0f, half }, normal, tangent, bitangent, { 0.0f, 0.0f } },
        { { half, 0.0f, half }, normal, tangent, bitangent, { 1.0f, 0.0f } },
        { { -half, 0.0f, -half }, normal, tangent, bitangent, { 0.0f, 1.0f } },
        { { half, 0.0f, -half }, normal, tangent, bitangent, { 1.0f, 1.0f } },
    };
    cache->Indices = { 0, 1, 3, 0, 3, 2 };
    SceneNode* plane = GeometryImport::CreateMeshNode("SkinTestPlane", cache, CacheDirectory);
    static_cast<MeshComponent*>(plane->GetComponent(MeshComponentType))->AssignMaterials({ SkinMaterial });
    plane->UploadToGPU();
}

void SkinTest::StartOnPlane()
{
    IsPlane = true;
    Chart = Skin::EnlistChart({ .MillimetresU = PlaneChartMillimetres, .MillimetresV = PlaneChartMillimetres, .BandTable = "skin" });
    SkinMaterial = MaterialRegistry::Enlist("skin");
    float chartValue = static_cast<float>(Chart);
    MaterialRegistry::SetParameter(SkinMaterial, "chart", { &chartValue, 1 });
    CreatePlane();

    float millimetresPerUnit = PlaneChartMillimetres / PlaneSize;
    XMFLOAT3 centre = { 3.0f, 0.0f, 2.0f };
    Stops = {
        { "whole chart", { 0.0f, 0.0f, 0.0f }, 0.0f, false },
        { "200 mm", centre, 200.0f, true },
        { "20 mm", centre, 20.0f, true },
        { "2 mm", centre, 2.0f, true },
    };
    for (Stop& stop : Stops)
        stop.Width /= millimetresPerUnit;

    Readback::Watch(DeferredPasses::GetAlbedoImage());
    Readback::Watch(DeferredPasses::GetNormalImage());
    IsRunning = true;
    StopIndex = 0;
    CurrentPhase = Phase::Settling;
    PhaseFrames = 0;
    Results.clear();
}

// Straight down on the stop's centre from the height that makes the view that wide, tilted a little
// so the look-at frame is defined. The near plane follows the height.
static void PlaceCamera(Camera* camera, const Stop& stop)
{
    Window* window = Renderer::GetWindow();
    float aspect = static_cast<float>(window->GetWidth()) / static_cast<float>((std::max)(window->GetHeight(), 1u));
    if (stop.Width <= 0.0f)
    {
        camera->SetNearPlane(Camera::DefaultNearPlane);
        camera->SetPositionFloat3({ 0.0f, 40.0f, -30.0f });
        camera->LookAtFloat3({ 0.0f, 0.0f, 0.0f });
        return;
    }
    float height = stop.Width / (2.0f * aspect);
    camera->SetNearPlane((std::min)(Camera::DefaultNearPlane, 0.1f * height));
    camera->SetPositionFloat3({ stop.Centre.x, stop.Centre.y + height, stop.Centre.z - 0.05f * height });
    camera->LookAtFloat3(stop.Centre);
}

static void CopyRows(const ReadbackResult& result, uint32_t bytesPerTexel, std::vector<uint8_t>& out)
{
    out.resize(static_cast<size_t>(result.Width) * result.Height * bytesPerTexel);
    for (uint32_t row = 0; row < result.Height; row++)
        std::memcpy(out.data() + static_cast<size_t>(row) * result.Width * bytesPerTexel, result.Data + static_cast<size_t>(row) * result.RowPitch,
                    static_cast<size_t>(result.Width) * bytesPerTexel);
}

static void RequestCapture(Capture& capture)
{
    capture = {};
    Window* window = Renderer::GetWindow();
    CaptureWidth = window->GetWidth();
    CaptureHeight = window->GetHeight();
    RHIStructures::ImageRegion region = { 0, 0, 0, 0, CaptureWidth, CaptureHeight };
    Readback::Request(DeferredPasses::GetAlbedoImage(), region, [&capture](const ReadbackResult& result)
    {
        CopyRows(result, 4, capture.Albedo);
        capture.HasAlbedo = true;
    });
    Readback::Request(DeferredPasses::GetNormalImage(), region, [&capture](const ReadbackResult& result)
    {
        CopyRows(result, 4, capture.Normal);
        capture.HasNormal = true;
    });
}

static double Percentile(std::vector<float>& values, double fraction)
{
    if (values.empty())
        return 0.0;
    size_t index = (std::min)(values.size() - 1, static_cast<size_t>(fraction * values.size()));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

// Albedo: the largest difference among red, green and blue in 8-bit levels. Normal: the largest
// difference of the two octahedral coordinates.
// -skin-diff: the normal difference of each stop as a 24-bit BMP in Cache/, magnified 20 times.
static void WriteDifferenceImage(const std::string& name, const std::vector<float>& difference)
{
    std::ofstream file(std::string(CacheDirectory) + "/skin_difference_" + name + ".bmp", std::ios::binary);
    uint32_t rowBytes = (CaptureWidth * 3 + 3) & ~3u;
    uint32_t imageBytes = rowBytes * CaptureHeight;
    uint8_t header[54] = { 'B', 'M' };
    auto put = [&header](int offset, uint32_t value) { std::memcpy(header + offset, &value, 4); };
    put(2, 54 + imageBytes);
    put(10, 54);
    put(14, 40);
    put(18, CaptureWidth);
    put(22, static_cast<uint32_t>(-static_cast<int32_t>(CaptureHeight)));
    header[26] = 1;
    header[28] = 24;
    put(34, imageBytes);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    std::vector<uint8_t> row(rowBytes, 0);
    for (uint32_t y = 0; y < CaptureHeight; y++)
    {
        for (uint32_t x = 0; x < CaptureWidth; x++)
        {
            uint8_t value = static_cast<uint8_t>((std::min)(255.0f, difference[static_cast<size_t>(y) * CaptureWidth + x] * 20.0f * 255.0f));
            row[x * 3] = row[x * 3 + 1] = row[x * 3 + 2] = value;
        }
        file.write(reinterpret_cast<const char*>(row.data()), rowBytes);
    }
}

static void CompareCaptures(StopResult& result)
{
    size_t pixels = static_cast<size_t>(CaptureWidth) * CaptureHeight;
    std::vector<float> albedo(pixels), normal(pixels);
    double albedoSum = 0.0, normalSum = 0.0;
    for (size_t i = 0; i < pixels; i++)
    {
        int largest = 0;
        for (int c = 0; c < 3; c++)
            largest = (std::max)(largest, std::abs(static_cast<int>(Wanted.Albedo[i * 4 + c]) - static_cast<int>(Coarse.Albedo[i * 4 + c])));
        albedo[i] = static_cast<float>(largest);

        const uint16_t* a = reinterpret_cast<const uint16_t*>(Wanted.Normal.data()) + i * 2;
        const uint16_t* b = reinterpret_cast<const uint16_t*>(Coarse.Normal.data()) + i * 2;
        normal[i] = (std::max)(std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0])), std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1]))) / 65535.0f;
        albedoSum += albedo[i];
        normalSum += normal[i];
    }
    if (std::wcsstr(GetCommandLineW(), L" -skin-diff"))
        WriteDifferenceImage(result.Name.substr(0, result.Name.find(' ')), normal);
    result.AlbedoMean = albedoSum / pixels;
    result.NormalMean = normalSum / pixels;
    result.AlbedoPercentile = Percentile(albedo, 0.99);
    result.NormalPercentile = Percentile(normal, 0.99);
}

static void BeginStop(Camera* camera)
{
    PlaceCamera(camera, Stops[StopIndex]);
    ArrivalFrame = Renderer::GetExecutor()->GetFrameNumber();
    LastMissingFrame = ArrivalFrame;
    CurrentPhase = Phase::Settling;
    PhaseFrames = 0;
    StopResult result;
    result.Name = Stops[StopIndex].Name;
    result.IsSkinFilling = Stops[StopIndex].IsSkinFilling;
    Results.push_back(result);
}

void SkinTest::HoldAtStop(int stop)
{
    HeldStop = stop;
}

void SkinTest::Update(float, Camera* camera)
{
    if (IsRunning && HeldStop >= 0)
    {
        PlaceCamera(camera, Stops[(std::min)(static_cast<size_t>(HeldStop), Stops.size() - 1)]);
        return;
    }
    if (!IsRunning || CurrentPhase == Phase::Finished)
        return;
    if (Results.size() <= StopIndex)
        BeginStop(camera);
    PlaceCamera(camera, Stops[StopIndex]);

    StopResult& result = Results.back();
    uint64_t frame = Renderer::GetExecutor()->GetFrameNumber();
    PhaseFrames++;
    switch (CurrentPhase)
    {
    case Phase::Settling:
        if (Paging::GetStats().Requests > 0)
            LastMissingFrame = frame;
        if ((PhaseFrames > MinimumSettleFrames && Paging::IsViewFullyResident()) || PhaseFrames > MaximumSettleFrames)
        {
            result.IsConverged = PhaseFrames <= MaximumSettleFrames;
            result.Convergence = static_cast<uint32_t>(LastMissingFrame - ArrivalFrame);
            result.MemoryBytes = CacheBytes();
            CurrentPhase = Phase::Measuring;
            PhaseFrames = 0;
            GBufferSum = FrameSum = 0.0;
            PagesBakedSum = 0;
        }
        break;

    case Phase::Measuring:
        GBufferSum += GpuMilliseconds("G-buffer");
        FrameSum += Renderer::GetFrameStats().FrameTimeMilliseconds;
        PagesBakedSum += Skin::GetStats().PagesBaked;
        if (PhaseFrames >= MeasuredFrames)
        {
            result.GBufferMilliseconds = GBufferSum / MeasuredFrames;
            result.FrameMilliseconds = FrameSum / MeasuredFrames;
            result.PagesBaked = PagesBakedSum;
            RequestCapture(Wanted);
            CurrentPhase = Phase::CapturingWanted;
            PhaseFrames = 0;
        }
        break;

    case Phase::CapturingWanted:
        if (Wanted.HasAlbedo && Wanted.HasNormal)
        {
            Paging::SetLevelBias(BiasLevels);
            CurrentPhase = Phase::Biasing;
            PhaseFrames = 0;
        }
        break;

    case Phase::Biasing:
        if (PhaseFrames >= BiasFrames)
        {
            RequestCapture(Coarse);
            CurrentPhase = Phase::CapturingCoarse;
            PhaseFrames = 0;
        }
        break;

    case Phase::CapturingCoarse:
        if (Coarse.HasAlbedo && Coarse.HasNormal)
        {
            Paging::SetLevelBias(0);
            CompareCaptures(result);
            std::cout << std::fixed << std::setprecision(3) << "Skin stop " << result.Name << ": " << (result.IsConverged ? "converged in " : "not converged after ")
                      << result.Convergence << " frames, G-buffer " << result.GBufferMilliseconds << " ms, frame " << result.FrameMilliseconds << " ms, "
                      << result.PagesBaked << " pages baked while measuring; albedo difference mean " << result.AlbedoMean << ", 99th percentile "
                      << result.AlbedoPercentile << "; normal difference mean " << result.NormalMean << ", 99th percentile " << result.NormalPercentile
                      << std::defaultfloat << std::endl;
            StopIndex++;
            if (StopIndex >= Stops.size())
            {
                CurrentPhase = Phase::Finished;
                PostQuitMessage(0);
            }
            else
                BeginStop(camera);
        }
        break;

    case Phase::Finished:
        break;
    }
}

void SkinTest::PrintResults()
{
    if (!IsRunning || HeldStop >= 0)
        return;
    if (CurrentPhase != Phase::Finished)
    {
        std::cout << "Skin test: failed, stopped at stop " << StopIndex + 1 << " of " << Stops.size() << std::endl;
        return;
    }

    std::ostringstream line;
    bool isInvariant = true;
    double albedoMean = 0.0, albedoPercentile = 0.0, normalMean = 0.0, normalPercentile = 0.0;
    double fastest = 1e30, slowest = 0.0;
    uint32_t longestConvergence = 0;
    bool isConverged = true;
    for (const StopResult& result : Results)
    {
        isInvariant &= result.AlbedoMean <= AlbedoMeanTolerance && result.AlbedoPercentile <= AlbedoPercentileTolerance &&
                       result.NormalMean <= NormalMeanTolerance && result.NormalPercentile <= NormalPercentileTolerance;
        albedoMean = (std::max)(albedoMean, result.AlbedoMean);
        albedoPercentile = (std::max)(albedoPercentile, result.AlbedoPercentile);
        normalMean = (std::max)(normalMean, result.NormalMean);
        normalPercentile = (std::max)(normalPercentile, result.NormalPercentile);
        longestConvergence = (std::max)(longestConvergence, result.Convergence);
        isConverged &= result.IsConverged;
        if (result.IsSkinFilling)
        {
            fastest = (std::min)(fastest, result.GBufferMilliseconds);
            slowest = (std::max)(slowest, result.GBufferMilliseconds);
        }
    }
    double spread = fastest > 0.0 ? (slowest - fastest) / fastest : 0.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Skin test: level invariance " << (isInvariant ? "held" : "FAILED") << " at " << Results.size() << " stops; largest albedo difference mean "
              << albedoMean << " (at most " << AlbedoMeanTolerance << "), 99th percentile " << albedoPercentile << " (at most " << AlbedoPercentileTolerance
              << ") in 8-bit levels; normal mean " << normalMean << " (at most " << NormalMeanTolerance << "), 99th percentile " << normalPercentile
              << " (at most " << NormalPercentileTolerance << ")" << std::endl;
    std::cout << "Skin test: memory " << (Results.front().MemoryBytes == Results.back().MemoryBytes ? "fixed" : "CHANGED") << ", "
              << Results.front().MemoryBytes / 1048576.0 << " MB at the first stop and " << Results.back().MemoryBytes / 1048576.0 << " MB at the last" << std::endl;
    std::cout << "Skin test: fragment cost, G-buffer " << fastest << " to " << slowest << " ms where skin fills the view, spread "
              << spread * 100.0 << "% (" << (spread < FragmentCostSpread ? "under" : "OVER") << " 25%; meaningful in Release with -novsync)" << std::endl;
    std::cout << "Skin test: features, " << Skin::GetStats().TotalCellsGenerated << " cells generated, cache peak "
              << Skin::GetStats().PeakResidentCells << " of " << Skin::GetStats().FeatureCacheBytes / (33 * 8) << " cells" << std::endl;
    std::cout << "Skin test: convergence, longest " << longestConvergence << " frames" << (isConverged ? "" : "; a stop DID NOT CONVERGE") << std::endl;
    if (IsPlane)
    {
        std::cout << "Skin test: seams, none on the plane" << std::endl;
        std::cout << "Skin test: stability, not run on the plane" << std::endl;
    }
    std::cout << std::defaultfloat;
}

void SkinTest::Shutdown()
{
    if (!IsRunning)
        return;
    Paging::SetLevelBias(0);
    Readback::Unwatch(DeferredPasses::GetAlbedoImage());
    Readback::Unwatch(DeferredPasses::GetNormalImage());
    MaterialRegistry::Retire(SkinMaterial);
    Skin::RetireChart(Chart);
    IsRunning = false;
}
