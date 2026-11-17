#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "PagingTest.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "imgui.h"
#include "Camera.h"
#include "MaterialRegistry.h"
#include "PageFile.h"
#include "Paging.h"
#include "Readback.h"
#include "Renderer.h"
#include "Geometry/GeometryImport.h"
#include "Resources/SceneComponent.h"
#include "Scene/SceneNode.h"

using namespace DirectX;

static constexpr uint32_t PatternSize = 16384;
static constexpr float PlaneSize = 60.0f;
static constexpr uint32_t MaxVerifiedFills = 300;
static constexpr float SettleSeconds = 4.0f;
static const std::string PageDirectory = "../../Cache/Pages/";
static const char* CacheDirectory = "../../Cache";

struct CameraKey
{
    float Time;
    XMFLOAT3 Position;
    XMFLOAT3 Target;
};

// The whole plane, a dolly to a close-up, a pan long enough to need more pages than the atlas
// holds, and a pull back.
static const CameraKey Path[] = {
    { 0.0f, { 0.0f, 40.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 3.0f, { 0.0f, 40.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 8.0f, { -20.0f, 0.5f, 4.0f }, { -20.0f, 0.0f, 6.0f } },
    { 16.0f, { 20.0f, 0.5f, 4.0f }, { 20.0f, 0.0f, 6.0f } },
    { 20.0f, { 10.0f, 20.0f, -15.0f }, { 10.0f, 0.0f, 5.0f } },
};

static std::unique_ptr<FilePageProducer> Producer;
static PagedTextureID Texture = 0;
static MaterialHandle PlaneMaterial = {};
static PageFile Tiles;
static uint32_t TextureSize = 0;
static bool IsRunning = false;
static bool HasQuit = false;
static float Seconds = 0.0f;
static uint32_t VerificationsRequested = 0;
static uint32_t PagesVerified = 0;
static std::string Failure;

struct Timing
{
    double Sum = 0.0;
    double Peak = 0.0;

    void Add(double milliseconds)
    {
        Sum += milliseconds;
        Peak = (std::max)(Peak, milliseconds);
    }
};
static Timing FrameTimes;
static Timing UpdateTimes;
static Timing FillTimes;
static uint32_t TimedFrames = 0;

static double GpuMilliseconds(const char* pass)
{
    for (const GpuPassTime& time : Renderer::GetFrameStats().GpuPasses)
        if (time.Name == pass)
            return time.Milliseconds;
    return 0.0;
}

static SceneNode* CreatePlane()
{
    float half = PlaneSize * 0.5f;
    VertexCache* cache = new VertexCache();
    XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    XMFLOAT3 tangent = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 bitangent = { 0.0f, 0.0f, 1.0f };
    cache->Vertices = {
        { { -half, 0.0f, half }, normal, tangent, bitangent, { 0.0f, 0.0f } },
        { { half, 0.0f, half }, normal, tangent, bitangent, { 1.0f, 0.0f } },
        { { -half, 0.0f, -half }, normal, tangent, bitangent, { 0.0f, 1.0f } },
        { { half, 0.0f, -half }, normal, tangent, bitangent, { 1.0f, 1.0f } },
    };
    cache->Indices = { 0, 1, 3, 0, 3, 2 };

    SceneNode* plane = GeometryImport::CreateMeshNode("PagingTestPlane", cache, CacheDirectory);
    static_cast<MeshComponent*>(plane->GetComponent(MeshComponentType))->AssignMaterials({ PlaneMaterial });
    plane->UploadToGPU();
    return plane;
}

static void CompareTile(const PageKey& page, const ReadbackResult& result)
{
    if (!Failure.empty())
        return;
    std::vector<uint8_t> tile(PageFile::TileBytes);
    Tiles.ReadTile(page.Level, page.X, page.Y, tile.data());
    for (uint32_t row = 0; row < PageFile::TileSize; row++)
        if (std::memcmp(result.Data + static_cast<size_t>(row) * result.RowPitch, tile.data() + static_cast<size_t>(row) * PageFile::TileSize * 4,
                        PageFile::TileSize * 4) != 0)
        {
            Failure = "page " + std::to_string(page.Level) + "/" + std::to_string(page.X) + "/" + std::to_string(page.Y) +
                      " differs from its tile in row " + std::to_string(row);
            return;
        }
    PagesVerified++;
}

void PagingTest::Start(const std::string& imagePath)
{
    std::string pagePath = PageDirectory + "test_pattern.pages";
    if (!imagePath.empty())
    {
        pagePath = PageDirectory + std::filesystem::path(imagePath).stem().string() + ".pages";
        PageFile::Build(imagePath, pagePath);
    }
    else
    {
        if (!std::filesystem::exists(pagePath))
            std::cout << "Paging test: building " << pagePath << " (about 1.6 GB), once" << std::endl;
        PageFile::BuildTestPattern(pagePath, PatternSize);
    }
    if (!Tiles.Open(pagePath))
        throw std::runtime_error("Cannot open " + pagePath);

    Producer = std::make_unique<FilePageProducer>(pagePath);
    TextureSize = Paging::PageSize << (Producer->GetLevelCount() - 1);
    Texture = Paging::EnlistTexture({ .LevelCount = Producer->GetLevelCount() }, Producer.get());
    PlaneMaterial = MaterialRegistry::Enlist("paging_test");
    float textureValue = static_cast<float>(Texture);
    MaterialRegistry::SetParameter(PlaneMaterial, "paged_texture", { &textureValue, 1 });
    CreatePlane();

    Readback::Watch(Paging::GetAtlasImage(0));
    Paging::SetFillObserver([](const PageKey& page, const PageSlot& slot)
    {
        if (page.Texture != Texture || VerificationsRequested >= MaxVerifiedFills)
            return;
        VerificationsRequested++;
        Readback::Request(Paging::GetAtlasImage(0), { 0, 0, slot.X, slot.Y, PageFile::TileSize, PageFile::TileSize },
            [page](const ReadbackResult& result) { CompareTile(page, result); });
    });
    IsRunning = true;
}

static float Smooth(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void PagingTest::Update(float dt, Camera* camera)
{
    if (!IsRunning)
        return;
    Seconds += dt;
    if (Seconds > 1.0f)
    {
        FrameTimes.Add(Renderer::GetFrameStats().FrameTimeMilliseconds);
        UpdateTimes.Add(Paging::GetStats().UpdateMilliseconds);
        FillTimes.Add(GpuMilliseconds("Paging fill"));
        TimedFrames++;
    }

    const size_t keyCount = std::size(Path);
    size_t next = 1;
    while (next < keyCount - 1 && Path[next].Time < Seconds)
        next++;
    const CameraKey& from = Path[next - 1];
    const CameraKey& to = Path[next];
    float t = Smooth((Seconds - from.Time) / (to.Time - from.Time));
    XMVECTOR position = XMVectorLerp(XMLoadFloat3(&from.Position), XMLoadFloat3(&to.Position), t);
    XMVECTOR target = XMVectorLerp(XMLoadFloat3(&from.Target), XMLoadFloat3(&to.Target), t);
    camera->SetPositionVector(position);
    camera->LookAtVector(target);

    if (!HasQuit && Seconds > Path[keyCount - 1].Time + SettleSeconds)
    {
        PostQuitMessage(0);
        HasQuit = true;
    }
}

void PagingTest::PrintResults()
{
    if (!IsRunning)
        return;

    const PagingStats& stats = Paging::GetStats();
    std::string result;
    if (!Failure.empty())
        result = "failed, " + Failure;
    else if (PagesVerified == 0)
        result = "failed, no filled page was read back";
    else if (stats.PeakResidentPages > stats.Slots)
        result = "failed, " + std::to_string(stats.PeakResidentPages) + " pages resident in " + std::to_string(stats.Slots) + " slots";
    else if (!HasQuit)
        result = "failed, the camera path did not finish";
    else if (!Paging::IsViewFullyResident())
        result = "failed, final view not fully resident (" + std::to_string(stats.Requests) + " pages missing)";

    if (!result.empty())
    {
        std::cout << "Paging test: " << result << std::endl;
        return;
    }
    std::cout << "Paging test: " << TextureSize << " x " << TextureSize << ", " << PageFile::CountPages(Producer->GetLevelCount()) << " pages, "
              << stats.Slots << " slots; " << stats.TotalFills << " filled, " << stats.TotalEvictions << " evicted, peak "
              << stats.PeakResidentPages << " resident; longest wait " << stats.LongestWait << " frames; " << PagesVerified
              << " pages verified; final view fully resident" << std::endl;
    if (TimedFrames > 0)
        std::cout << "Paging timing over " << TimedFrames << " frames, average and peak: frame " << FrameTimes.Sum / TimedFrames << " and "
                  << FrameTimes.Peak << " ms; paging update CPU " << UpdateTimes.Sum / TimedFrames << " and " << UpdateTimes.Peak
                  << " ms; fill GPU " << FillTimes.Sum / TimedFrames << " and " << FillTimes.Peak << " ms" << std::endl;
}

void PagingTest::Shutdown()
{
    if (!IsRunning)
        return;
    Paging::SetFillObserver(nullptr);
    Readback::Unwatch(Paging::GetAtlasImage(0));
    Paging::RetireTexture(Texture);
    Producer.reset();
    MaterialRegistry::Retire(PlaneMaterial);
    IsRunning = false;
}

void PagingTest::DrawPanel()
{
    const PagingStats& stats = Paging::GetStats();
    ImGui::Text("Textures %u, resident %u of %u slots", stats.Textures, stats.ResidentPages, stats.Slots);
    ImGui::Text("Requests %u, fills %u, evictions %u", stats.Requests, stats.Fills, stats.Evictions);
    ImGui::Text("IO queue %u, unplaced %u", stats.QueuedPages, stats.UnplacedPages);
    ImGui::Text("Feedback latency %u frames, fill GPU %.3f ms", stats.FeedbackLatency, GpuMilliseconds("Paging fill"));
    ImGui::Text("Update CPU %.3f ms", stats.UpdateMilliseconds);
}
