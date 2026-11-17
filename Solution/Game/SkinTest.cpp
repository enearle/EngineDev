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
#include "imgui.h"
#include "Camera.h"
#include "Animation/AnimationSystem.h"
#include "Animation/AnimatorComponent.h"
#include "Animation/Skeleton.h"
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
static constexpr uint32_t AnimatedSettleFrames = 30;
static constexpr uint32_t AnimatedFrames = 120;
static constexpr float SeamViewMillimetres = 60.0f;
static constexpr float SeamSampleMillimetres = 40.0f;      // Seam vertices this near the target are sampled
static constexpr float SeamVertexWeight = 0.99f;            // PatchWeight of a vertex on a seam
static constexpr int SeamClearance = 4;                     // Pixels from a seam sample that are not "beside" it
static constexpr int SeamBeside = 10;                       // Pixels to the samples beside each seam sample
static constexpr double SeamStepTolerance = 1.25;           // Seam mean step over the mean step beside it,
static constexpr double SeamAlbedoFloor = 0.25;             // plus these: steps are mostly under one 8-bit
static constexpr double SeamNormalFloor = 0.002;            // level, where a ratio alone measures quantisation
static const char* CacheDirectory = "../../Cache";

enum class Aim
{
    PlaneCentre,
    WholeCharacter,
    Torso,
    Forearm,
    Seam
};

struct Stop
{
    std::string Name;
    XMFLOAT3 Centre;                // On the plane
    float Width;                    // Of the view in world units; 0 for the whole chart or character
    bool IsSkinFilling;
    Aim Target = Aim::PlaneCentre;
    bool IsAnimated = false;        // A clip plays after the checks, for the stability line
    bool IsSeam = false;            // Seam steps are measured after the checks, with and without patches
};

// A vertex of the character's skin the camera looks at, followed through skinning.
struct SurfaceTarget
{
    RHIStructures::SkinnedVertex Vertex;
    bool IsValid = false;
};

enum class Phase
{
    Settling,
    Measuring,
    CapturingWanted,
    Biasing,
    CapturingCoarse,
    Unpatching,
    CapturingUnpatched,
    Animating,
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
    uint32_t CellsGenerated = 0;
    uint32_t ResidentCells = 0;
};

// Steps between neighbouring pixels at seam samples and beside them: for each pixel the largest
// difference to its eight neighbours, then the largest of that over the 3 x 3 pixels around it, so a
// sample a pixel off the seam still sees it. Beside a sample are the pixels SeamBeside away up, down,
// left and right that are clear of the seam, so both sides of the comparison see the same region.
struct SeamSteps
{
    uint32_t Samples = 0;
    double SeamAlbedo = 0.0;            // Means; albedo in 8-bit levels, normal in octahedral units
    double SeamNormal = 0.0;
    double BesideAlbedo = 0.0;
    double BesideNormal = 0.0;
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
static SceneNode* Character = nullptr;
static AnimatorComponent* CharacterAnimator = nullptr;
static SurfaceTarget TorsoTarget;
static SurfaceTarget ForearmTarget;
static float WorldPerMillimetre = 0.0f;
static uint32_t StabilityFrames = 0;
static uint32_t StabilityFills = 0;
static uint32_t StabilityFramesWithFills = 0;
static bool HasStabilityRun = false;
static SurfaceTarget SeamTarget;
static std::vector<RHIStructures::SkinnedVertex> SeamVertices;
static bool HasSeamRun = false;
static SeamSteps PatchedSteps;
static SeamSteps UnpatchedSteps;

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

static XMVECTOR JointWorldPosition(const Skeleton& skeleton, const char* name, const XMMATRIX& world)
{
    int32_t joint = skeleton.FindJoint(name);
    if (joint < 0)
        return world.r[3];
    return XMVector3TransformCoord(XMMatrixInverse(nullptr, XMLoadFloat4x4(&skeleton.GetJoint(joint).InverseBind)).r[3], world);
}

// The skinned vertex nearest a point among a material's vertices whose normal faces a direction, in
// the bind pose. Vertices are read before upload drops them.
static SurfaceTarget FindSurfaceVertex(SceneNode* node, const std::string& material, XMVECTOR point, XMVECTOR facing, const XMMATRIX& world,
                                       float& bestDistance, float& millimetresPerUnit, bool isOnSeam = false)
{
    SurfaceTarget best;
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
        {
            Mesh& mesh = asset->GetMesh(i);
            if (!mesh.IsSkinned() || (!material.empty() && mesh.GetMaterialName() != material))
                continue;
            const RHIStructures::SkinnedVertex* vertices = static_cast<const RHIStructures::SkinnedVertex*>(mesh.GetCachedVertexData());
            for (uint32_t v = 0; v < mesh.GetVertexCount(); v++)
            {
                XMVECTOR position = XMVector3TransformCoord(XMLoadFloat3(&vertices[v].Position), world);
                XMVECTOR normal = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&vertices[v].Normal), world));
                float distance = XMVectorGetX(XMVector3Length(position - point));
                if (XMVectorGetX(XMVector3Dot(normal, facing)) < 0.6f || distance >= bestDistance || (isOnSeam && vertices[v].PatchWeight < SeamVertexWeight))
                    continue;
                bestDistance = distance;
                best.Vertex = vertices[v];
                best.IsValid = true;
                millimetresPerUnit = mesh.GetChart().MillimetresPerUnit;
            }
        }
    }
    for (SceneNode* child : node->GetChildren())
    {
        SurfaceTarget found = FindSurfaceVertex(child, material, point, facing, world, bestDistance, millimetresPerUnit, isOnSeam);
        if (found.IsValid)
            best = found;
    }
    return best;
}

// Seam vertices of every skinned mesh within a distance of a point, in the bind pose.
static void CollectSeamVertices(SceneNode* node, XMVECTOR point, float within, const XMMATRIX& world)
{
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
        {
            Mesh& mesh = asset->GetMesh(i);
            if (!mesh.IsSkinned())
                continue;
            const RHIStructures::SkinnedVertex* vertices = static_cast<const RHIStructures::SkinnedVertex*>(mesh.GetCachedVertexData());
            for (uint32_t v = 0; v < mesh.GetVertexCount(); v++)
                if (vertices[v].PatchWeight >= SeamVertexWeight &&
                    XMVectorGetX(XMVector3Length(XMVector3TransformCoord(XMLoadFloat3(&vertices[v].Position), world) - point)) <= within)
                    SeamVertices.push_back(vertices[v]);
        }
    }
    for (SceneNode* child : node->GetChildren())
        CollectSeamVertices(child, point, within, world);
}

// A skin vertex's position and normal now, skinned on the CPU with the animator's palette.
static void SkinVertex(const RHIStructures::SkinnedVertex& vertex, XMVECTOR& position, XMVECTOR& normal)
{
    const std::vector<XMFLOAT4X4>& palette = CharacterAnimator->GetPalette();
    const float weights[4] = { vertex.BoneWeights.x, vertex.BoneWeights.y, vertex.BoneWeights.z, vertex.BoneWeights.w };
    const uint32_t joints[4] = { vertex.BoneIndices.x, vertex.BoneIndices.y, vertex.BoneIndices.z, vertex.BoneIndices.w };
    XMMATRIX skin = XMMatrixIdentity();
    if (weights[0] + weights[1] + weights[2] + weights[3] > 0.0f && !palette.empty())
    {
        skin = XMMATRIX(XMVectorZero(), XMVectorZero(), XMVectorZero(), XMVectorZero());
        for (int i = 0; i < 4; i++)
            if (weights[i] > 0.0f && joints[i] < palette.size())
                skin += XMLoadFloat4x4(&palette[joints[i]]) * weights[i];
    }
    XMMATRIX toWorld = skin * Character->GetWorldMatrix();
    position = XMVector3TransformCoord(XMLoadFloat3(&vertex.Position), toWorld);
    normal = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&vertex.Normal), toWorld));
}

void SkinTest::StartOnCharacter(SceneNode* character)
{
    Character = character;
    CharacterAnimator = static_cast<AnimatorComponent*>(character->GetComponent(AnimatorComponentType));
    if (!CharacterAnimator || !CharacterAnimator->GetSkeleton())
    {
        std::cout << "Skin test: failed, the character has no skeleton" << std::endl;
        return;
    }

    XMMATRIX world = character->GetWorldMatrix();
    const Skeleton& skeleton = *CharacterAnimator->GetSkeleton();
    XMVECTOR towardCamera = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    float distance = 1e30f, millimetresPerUnit = 10.0f;
    TorsoTarget = FindSurfaceVertex(character, "Body", JointWorldPosition(skeleton, "spine2", world), towardCamera, world, distance, millimetresPerUnit);
    XMVECTOR forearm = XMVectorLerp(JointWorldPosition(skeleton, "l_forearm", world), JointWorldPosition(skeleton, "l_hand", world), 0.45f);
    distance = 1e30f;
    ForearmTarget = FindSurfaceVertex(character, "Arms", forearm, XMVector3Normalize(XMVectorSet(0.0f, 10.0f, -8.0f, 0.0f) - forearm), world,
                                      distance, millimetresPerUnit);
    if (!TorsoTarget.IsValid || !ForearmTarget.IsValid)
    {
        std::cout << "Skin test: failed, no Body or Arms skin vertex faces the camera" << std::endl;
        return;
    }
    WorldPerMillimetre = XMVectorGetX(XMVector3Length(world.r[0])) / millimetresPerUnit;

    // The neck's seam: a closed curve, so its patch has no ends.
    distance = 1e30f;
    float ignored = 0.0f;
    SeamTarget = FindSurfaceVertex(character, "", JointWorldPosition(skeleton, "neck1", world), towardCamera, world, distance, ignored, true);
    SeamVertices.clear();
    if (SeamTarget.IsValid)
    {
        XMVECTOR seamPoint = XMVector3TransformCoord(XMLoadFloat3(&SeamTarget.Vertex.Position), world);
        CollectSeamVertices(character, seamPoint, SeamSampleMillimetres * WorldPerMillimetre, world);
    }

    Stops = {
        { "whole body", {}, 0.0f, false, Aim::WholeCharacter },
        { "torso 400 mm", {}, 400.0f, false, Aim::Torso },
        { "forearm 200 mm", {}, 200.0f, false, Aim::Forearm },
        { "forearm 20 mm", {}, 20.0f, true, Aim::Forearm, true },
        { "forearm 2 mm", {}, 2.0f, true, Aim::Forearm },
    };
    if (SeamTarget.IsValid)
        Stops.push_back({ "neck seam 60 mm", {}, SeamViewMillimetres, false, Aim::Seam, false, true });
    for (Stop& stop : Stops)
        stop.Width *= WorldPerMillimetre;

    Readback::Watch(DeferredPasses::GetAlbedoImage());
    Readback::Watch(DeferredPasses::GetNormalImage());
    IsRunning = true;
    StopIndex = 0;
    CurrentPhase = Phase::Settling;
    PhaseFrames = 0;
    Results.clear();
}

// On the plane: straight down on the stop's centre from the height that makes the view that wide,
// tilted a little so the look-at frame is defined. On the character: along the target's normal.
// The near plane follows the distance.
static void PlaceCamera(Camera* camera, const Stop& stop)
{
    Window* window = Renderer::GetWindow();
    float aspect = static_cast<float>(window->GetWidth()) / static_cast<float>((std::max)(window->GetHeight(), 1u));
    if (stop.Target == Aim::WholeCharacter)
    {
        const XMFLOAT4& bounds = CharacterAnimator->GetModelBounds();
        XMMATRIX world = Character->GetWorldMatrix();
        XMVECTOR centre = XMVector3TransformCoord(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&bounds)), world);
        float radius = bounds.w * XMVectorGetX(XMVector3Length(world.r[0]));
        XMFLOAT3 target, position;
        XMStoreFloat3(&target, centre);
        XMStoreFloat3(&position, centre + XMVectorSet(0.0f, 0.0f, -1.2f * radius, 0.0f));
        camera->SetNearPlane(Camera::DefaultNearPlane);
        camera->SetPositionFloat3(position);
        camera->LookAtFloat3(target);
        return;
    }
    if (stop.Target == Aim::Torso || stop.Target == Aim::Forearm || stop.Target == Aim::Seam)
    {
        XMVECTOR point, normal;
        const SurfaceTarget& target = stop.Target == Aim::Torso ? TorsoTarget : (stop.Target == Aim::Forearm ? ForearmTarget : SeamTarget);
        SkinVertex(target.Vertex, point, normal);
        float distance = stop.Width / (2.0f * aspect);
        camera->SetNearPlane((std::min)(Camera::DefaultNearPlane, 0.1f * distance));
        camera->SetPositionVector(point + normal * distance);
        camera->LookAtVector(point);
        return;
    }
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
    {
        std::string name = result.Name;
        std::replace(name.begin(), name.end(), ' ', '_');
        WriteDifferenceImage(name, normal);
    }
    result.AlbedoMean = albedoSum / pixels;
    result.NormalMean = normalSum / pixels;
    result.AlbedoPercentile = Percentile(albedo, 0.99);
    result.NormalPercentile = Percentile(normal, 0.99);
}

static bool IsSurfacePixel(const Capture& capture, int32_t x, int32_t y)
{
    return x >= 0 && y >= 0 && x < static_cast<int32_t>(CaptureWidth) && y < static_cast<int32_t>(CaptureHeight) &&
           capture.Albedo[(static_cast<size_t>(y) * CaptureWidth + x) * 4 + 3] != 0;
}

static SeamSteps MeasureSeamSteps(const Capture& capture, Camera* camera)
{
    int32_t width = static_cast<int32_t>(CaptureWidth), height = static_cast<int32_t>(CaptureHeight);
    size_t pixels = static_cast<size_t>(width) * height;
    std::vector<float> albedoStep(pixels, 0.0f), normalStep(pixels, 0.0f);
    std::vector<uint8_t> isComplete(pixels, 0);
    const uint16_t* normals = reinterpret_cast<const uint16_t*>(capture.Normal.data());
    for (int32_t y = 1; y + 1 < height; y++)
        for (int32_t x = 1; x + 1 < width; x++)
        {
            size_t i = static_cast<size_t>(y) * width + x;
            bool isAllSurface = true;
            for (int32_t dy = -1; dy <= 1; dy++)
                for (int32_t dx = -1; dx <= 1; dx++)
                {
                    if (!IsSurfacePixel(capture, x + dx, y + dy))
                    {
                        isAllSurface = false;
                        continue;
                    }
                    size_t j = static_cast<size_t>(y + dy) * width + x + dx;
                    for (int c = 0; c < 3; c++)
                        albedoStep[i] = (std::max)(albedoStep[i], static_cast<float>(std::abs(static_cast<int>(capture.Albedo[i * 4 + c]) - static_cast<int>(capture.Albedo[j * 4 + c]))));
                    for (int c = 0; c < 2; c++)
                        normalStep[i] = (std::max)(normalStep[i], std::abs(static_cast<int>(normals[i * 2 + c]) - static_cast<int>(normals[j * 2 + c])) / 65535.0f);
                }
            isComplete[i] = isAllSurface ? 1 : 0;
        }
    auto around = [&](const std::vector<float>& steps, int32_t x, int32_t y)
    {
        float largest = 0.0f;
        for (int32_t dy = -1; dy <= 1; dy++)
            for (int32_t dx = -1; dx <= 1; dx++)
                largest = (std::max)(largest, steps[static_cast<size_t>(y + dy) * width + x + dx]);
        return largest;
    };
    auto isUsable = [&](int32_t x, int32_t y)
    {
        if (x < 2 || y < 2 || x + 2 >= width || y + 2 >= height)
            return false;
        for (int32_t dy = -1; dy <= 1; dy++)
            for (int32_t dx = -1; dx <= 1; dx++)
                if (!isComplete[static_cast<size_t>(y + dy) * width + x + dx])
                    return false;
        return true;
    };

    // Seam samples: the seam vertices, skinned and projected; each pixel counts once.
    SeamSteps steps;
    XMMATRIX viewProjection = camera->GetView() * camera->GetProjection();
    std::vector<uint8_t> isNearSeam(pixels, 0);
    std::vector<std::pair<int32_t, int32_t>> samples;
    for (const RHIStructures::SkinnedVertex& vertex : SeamVertices)
    {
        XMVECTOR position, normal;
        SkinVertex(vertex, position, normal);
        XMVECTOR clip = XMVector4Transform(XMVectorSetW(position, 1.0f), viewProjection);
        float w = XMVectorGetW(clip);
        if (w <= 0.0f)
            continue;
        int32_t x = static_cast<int32_t>((XMVectorGetX(clip) / w * 0.5f + 0.5f) * width);
        int32_t y = static_cast<int32_t>((0.5f - XMVectorGetY(clip) / w * 0.5f) * height);
        if (x < 0 || y < 0 || x >= width || y >= height)
            continue;
        for (int32_t dy = -SeamClearance; dy <= SeamClearance; dy++)
            for (int32_t dx = -SeamClearance; dx <= SeamClearance; dx++)
                if (x + dx >= 0 && y + dy >= 0 && x + dx < width && y + dy < height)
                    isNearSeam[static_cast<size_t>(y + dy) * width + x + dx] = 1;
        samples.push_back({ x, y });
    }
    std::sort(samples.begin(), samples.end());
    samples.erase(std::unique(samples.begin(), samples.end()), samples.end());

    uint64_t beside = 0;
    for (const auto& [x, y] : samples)
    {
        if (!isUsable(x, y))
            continue;
        steps.SeamAlbedo += around(albedoStep, x, y);
        steps.SeamNormal += around(normalStep, x, y);
        steps.Samples++;
        const int32_t offsets[4][2] = { { SeamBeside, 0 }, { -SeamBeside, 0 }, { 0, SeamBeside }, { 0, -SeamBeside } };
        for (const auto& offset : offsets)
        {
            int32_t bx = x + offset[0], by = y + offset[1];
            if (!isUsable(bx, by) || isNearSeam[static_cast<size_t>(by) * width + bx])
                continue;
            steps.BesideAlbedo += around(albedoStep, bx, by);
            steps.BesideNormal += around(normalStep, bx, by);
            beside++;
        }
    }
    if (steps.Samples > 0)
    {
        steps.SeamAlbedo /= steps.Samples;
        steps.SeamNormal /= steps.Samples;
    }
    if (beside > 0)
    {
        steps.BesideAlbedo /= static_cast<double>(beside);
        steps.BesideNormal /= static_cast<double>(beside);
    }
    return steps;
}

static bool IsSeamHeld(const SeamSteps& steps)
{
    return steps.Samples > 0 && steps.SeamAlbedo <= SeamStepTolerance * steps.BesideAlbedo + SeamAlbedoFloor &&
           steps.SeamNormal <= SeamStepTolerance * steps.BesideNormal + SeamNormalFloor;
}

static void BeginStop(Camera* camera)
{
    if (Character)
        AnimationSystem::SetFrozen(true);
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

static void PrintStop(const StopResult& result);

static void FinishStop(Camera* camera)
{
    PrintStop(Results.back());
    StopIndex++;
    if (StopIndex >= Stops.size())
    {
        CurrentPhase = Phase::Finished;
        PostQuitMessage(0);
    }
    else
        BeginStop(camera);
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
    result.CellsGenerated += Skin::GetStats().CellsGenerated;
    result.ResidentCells = Skin::GetStats().ResidentCells;
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
            if (Stops[StopIndex].IsSeam)
            {
                PatchedSteps = MeasureSeamSteps(Wanted, camera);
                Skin::SetPatchesEnabled(false);
                CurrentPhase = Phase::Unpatching;
                PhaseFrames = 0;
            }
            else if (Stops[StopIndex].IsAnimated)
            {
                AnimationSystem::SetFrozen(false);
                CurrentPhase = Phase::Animating;
                PhaseFrames = 0;
            }
            else
                FinishStop(camera);
        }
        break;

    case Phase::Unpatching:
        if ((PhaseFrames > MinimumSettleFrames && Paging::IsViewFullyResident()) || PhaseFrames > MaximumSettleFrames)
        {
            RequestCapture(Coarse);
            CurrentPhase = Phase::CapturingUnpatched;
            PhaseFrames = 0;
        }
        break;

    case Phase::CapturingUnpatched:
        if (Coarse.HasAlbedo && Coarse.HasNormal)
        {
            UnpatchedSteps = MeasureSeamSteps(Coarse, camera);
            Skin::SetPatchesEnabled(true);
            HasSeamRun = true;
            FinishStop(camera);
        }
        break;

    case Phase::Animating:
        if (PhaseFrames > AnimatedSettleFrames)
        {
            uint32_t fills = Paging::GetStats().Fills;
            StabilityFills += fills;
            StabilityFramesWithFills += fills > 0 ? 1 : 0;
            StabilityFrames++;
        }
        if (PhaseFrames >= AnimatedSettleFrames + AnimatedFrames)
        {
            AnimationSystem::SetFrozen(true);
            HasStabilityRun = true;
            FinishStop(camera);
        }
        break;

    case Phase::Finished:
        break;
    }
}

static void PrintStop(const StopResult& result)
{
    std::cout << std::fixed << std::setprecision(3) << "Skin stop " << result.Name << ": " << (result.IsConverged ? "converged in " : "not converged after ")
              << result.Convergence << " frames, G-buffer " << result.GBufferMilliseconds << " ms, frame " << result.FrameMilliseconds << " ms, "
              << result.PagesBaked << " pages baked while measuring, " << result.CellsGenerated << " cells generated, " << result.ResidentCells
              << " cells resident; albedo difference mean " << result.AlbedoMean << ", 99th percentile "
              << result.AlbedoPercentile << "; normal difference mean " << result.NormalMean << ", 99th percentile " << result.NormalPercentile
              << std::defaultfloat << std::endl;
}

void SkinTest::DrawPanel()
{
    const SkinStats& stats = Skin::GetStats();
    ImGui::Text("Charts %u, chart maps %.1f MB", stats.Charts, stats.ChartMapBytes / (1024.0 * 1024.0));
    ImGui::Text("Pages baked %u this frame, %llu in all", stats.PagesBaked, static_cast<unsigned long long>(stats.TotalPagesBaked));
    ImGui::Text("Cells generated %u this frame, %llu in all", stats.CellsGenerated, static_cast<unsigned long long>(stats.TotalCellsGenerated));
    ImGui::Text("Feature cache %u of %u cells, peak %u, %.1f MB", stats.ResidentCells, stats.CellCapacity, stats.PeakResidentCells,
                stats.FeatureCacheBytes / (1024.0 * 1024.0));
    ImGui::Text("Feature pass GPU %.3f ms, bake and copy (Paging fill) GPU %.3f ms", GpuMilliseconds("Skin features"), GpuMilliseconds("Paging fill"));
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
    size_t failedStops = 0;
    double albedoMean = 0.0, albedoPercentile = 0.0, normalMean = 0.0, normalPercentile = 0.0;
    double fastest = 1e30, slowest = 0.0;
    uint32_t longestConvergence = 0;
    bool isConverged = true;
    for (const StopResult& result : Results)
    {
        bool isStopInvariant = result.AlbedoMean <= AlbedoMeanTolerance && result.AlbedoPercentile <= AlbedoPercentileTolerance &&
                               result.NormalMean <= NormalMeanTolerance && result.NormalPercentile <= NormalPercentileTolerance;
        isInvariant &= isStopInvariant;
        failedStops += isStopInvariant ? 0 : 1;
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
    std::cout << "Skin test: level invariance " << (isInvariant ? "held at " : "FAILED at ") << (isInvariant ? Results.size() : failedStops) << " of "
              << Results.size() << " stops; largest albedo difference mean "
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
    else
    {
        if (!HasSeamRun)
            std::cout << "Skin test: seams, not measured (" << (SeamTarget.IsValid ? "DID NOT RUN" : "no seam vertex near the neck") << ")" << std::endl;
        else
            std::cout << "Skin test: seams " << (IsSeamHeld(PatchedSteps) ? "held" : "FAILED") << " at " << PatchedSteps.Samples
                      << " seam pixels; step to neighbouring pixels, albedo " << PatchedSteps.SeamAlbedo << " on the seam and " << PatchedSteps.BesideAlbedo
                      << " beside it, normal " << PatchedSteps.SeamNormal << " and " << PatchedSteps.BesideNormal << " (seam at most "
                      << SeamStepTolerance << " times beside, plus " << SeamAlbedoFloor << " and " << SeamNormalFloor << "); without patches "
                      << (IsSeamHeld(UnpatchedSteps) ? "it would hold" : "it would fail") << ", albedo " << UnpatchedSteps.SeamAlbedo << " and "
                      << UnpatchedSteps.BesideAlbedo << ", normal " << UnpatchedSteps.SeamNormal << " and " << UnpatchedSteps.BesideNormal << std::endl;
        std::cout << "Skin test: stability, " << StabilityFills << " pages filled in " << StabilityFrames << " frames with a clip playing and the camera "
                  << "following the skin, " << StabilityFramesWithFills << " frames with any" << (HasStabilityRun ? "" : " (DID NOT RUN)") << std::endl;
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
