#include "Renderer.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include "Window.h"
#include "GraphicsSettings.h"
#include "DrawList.h"
#include "MaterialRegistry.h"
#include "MeshRegistry.h"
#include "PassGraph.h"
#include "Readback.h"
#include "MipGenerator.h"
#include "Skinning.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"

using namespace DirectX;
using namespace RHIStructures;

static constexpr uint64_t FrameRingBytesPerFrame = 16ull * 1024 * 1024;

static PipelineExecutor* Executor = nullptr;
static BufferAllocator* Allocator = nullptr;
static Window* MainWindow = nullptr;
static Event<uint32_t, uint32_t> OnResize;
static FrameStats Stats;
static FrameStats StatsInProgress;
static std::chrono::steady_clock::time_point LastFrameStart;

static PassGraph* Graph = nullptr;
static RenderScene Scene;
static std::vector<PreparedObject> PreparedObjects;
static PipelineLayout* SceneLayout = nullptr;
static DescriptorSetHandle SceneSet = {};
static uint32_t FrameConstantsOffset = 0;

struct EnlistedExternalPass
{
    Renderer::ExternalPassID ID;
    ExternalPass Pass;
    PassID GraphPass;
};
static std::vector<EnlistedExternalPass> ExternalPasses;
static Renderer::ExternalPassID NextExternalPassID = 0;

static uint32_t GameViewX = 0, GameViewY = 0, GameViewW = 0, GameViewH = 0;

Window* Renderer::GetWindow() { return MainWindow; }
Event<uint32_t, uint32_t>& Renderer::EventOnResize() { return OnResize; }
const FrameStats& Renderer::GetFrameStats() { return Stats; }
PassGraph& Renderer::GetPassGraph() { return *Graph; }
RenderScene& Renderer::GetRenderScene() { return Scene; }
PipelineLayout* Renderer::GetSceneLayout() { return SceneLayout; }
const std::vector<PreparedObject>& Renderer::GetPreparedObjects() { return PreparedObjects; }
Format Renderer::GetBackBufferFormat() { return Executor->GetBackBufferFormat(); }
PipelineExecutor* Renderer::GetExecutor() { return Executor; }

void Renderer::SetGameViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    GameViewX = x; GameViewY = y; GameViewW = w; GameViewH = h;
}

void Renderer::GetGameViewport(uint32_t& x, uint32_t& y, uint32_t& w, uint32_t& h)
{
    x = GameViewX; y = GameViewY; w = GameViewW; h = GameViewH;
}

ShaderStageMask Renderer::GetSceneVisibleStages()
{
    ShaderStageMask stages(0);
    stages.SetVertex(true);
    stages.SetFragment(true);
    return stages;
}

std::vector<DescriptorBinding> Renderer::GetSceneSetBindings()
{
    return {
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = SceneSetIndex, .Count = 1, .Size = sizeof(FrameConstants) },
        { .Type = DescriptorType::StorageBuffer, .Slot = 1, .Set = SceneSetIndex, .Count = 1, .Stride = sizeof(InstanceData) },
    };
}

void Renderer::BindSceneSet(PipelineExecutor* executor)
{
    uint32_t offsets[] = { FrameConstantsOffset };
    executor->BindDescriptorSet(SceneSet, offsets);
}

void Renderer::ReportPassStats(const std::string& name, uint32_t instances, uint32_t batches, uint32_t drawCalls)
{
    StatsInProgress.Passes.push_back({ name, instances, batches, drawCalls });
    StatsInProgress.DrawCalls += drawCalls;
}

Renderer::ExternalPassID Renderer::EnlistExternalPass(ExternalPass pass)
{
    ExternalPassID id = NextExternalPassID++;

    PassDesc desc;
    desc.Name = pass.Name;
    desc.Kind = PassKind::Overlay;
    desc.Order = pass.Order;
    desc.Execute = [render = pass.Render](const PassContext&) { if (render) render(); };

    ExternalPasses.push_back({ id, std::move(pass), Graph->AddPass(std::move(desc)) });
    std::stable_sort(ExternalPasses.begin(), ExternalPasses.end(),
        [](const EnlistedExternalPass& a, const EnlistedExternalPass& b) { return a.Pass.Order < b.Pass.Order; });
    return id;
}

void Renderer::RetireExternalPass(ExternalPassID id)
{
    for (const EnlistedExternalPass& external : ExternalPasses)
        if (external.ID == id)
            Graph->RemovePass(external.GraphPass);
    std::erase_if(ExternalPasses, [id](const EnlistedExternalPass& pass) { return pass.ID == id; });
}

void Renderer::Start(Window* mainWindow)
{
    MainWindow = mainWindow;
    MainWindow->OnResize.Subscribe([](UINT width, UINT height) { OnResizeEnd(width, height); });

    CoreInitData data;
    Executor = PipelineExecutor::Create(MainWindow, data);
    Allocator = BufferAllocator::GetInstance();
    Allocator->CreateFrameRing(FrameRingBytesPerFrame);
    Graph = new PassGraph();
    Readback::Start();
    MipGenerator::Start();
    Skinning::Start();

    MaterialTypes::RegisterBuiltInTypes();
    ResourceLayout sceneResources;
    sceneResources.VisibleStages = GetSceneVisibleStages();
    sceneResources.Bindings = GetSceneSetBindings();
    for (const DescriptorBinding& binding : MaterialRegistry::GetMaterialSetBindings())
        sceneResources.Bindings.push_back(binding);

    ShaderStageMask constantStages(0);
    constantStages.SetVertex(true);
    SceneLayout = PipelineLayout::Create(sceneResources, { { .Size = sizeof(DrawConstants), .VisibleStages = constantStages } });

    BufferHandle ring = Allocator->GetFrameRing();
    SceneSet = Allocator->AllocateDescriptorSet(SceneLayout->GetID(), SceneSetIndex, {
        DescriptorSetBinding::ForBuffer(0, ring),
        DescriptorSetBinding::ForBuffer(1, ring),
    });
    MaterialRegistry::Start();

    MainWindow->Show();
}

static void PrepareFrameData()
{
    FrameConstants constants = {};
    if (Scene.HasCamera)
    {
        XMMATRIX viewProjection = XMLoadFloat4x4(&Scene.Camera.View) * XMLoadFloat4x4(&Scene.Camera.Projection);
        XMStoreFloat4x4(&constants.ViewProjection, viewProjection);
        XMStoreFloat4x4(&constants.InverseViewProjection, XMMatrixInverse(nullptr, viewProjection));
        constants.CameraPosition = { Scene.Camera.Position.x, Scene.Camera.Position.y, Scene.Camera.Position.z, 1.0f };
    }
    for (uint32_t i = 0; i < MaxLights && i < Scene.Lights.size(); i++)
    {
        constants.Lights[i] = Scene.Lights[i].Data;
        constants.Lights[i].ShadowIndex = i;
        constants.LightViewProjection[i] = Scene.Lights[i].ViewProjection;
    }

    FrameAllocation constantsAllocation = Allocator->AllocateFrameData(sizeof(FrameConstants), BufferAllocator::UniformAlignment);
    memcpy(constantsAllocation.Address, &constants, sizeof(FrameConstants));
    FrameConstantsOffset = static_cast<uint32_t>(constantsAllocation.Offset);

    uint32_t paletteBase = 0;
    if (!Scene.BoneMatrices.empty())
    {
        uint64_t size = Scene.BoneMatrices.size() * sizeof(XMFLOAT4X4);
        FrameAllocation bones = Allocator->AllocateFrameData(size, sizeof(XMFLOAT4X4));
        memcpy(bones.Address, Scene.BoneMatrices.data(), size);
        paletteBase = static_cast<uint32_t>(bones.Offset / sizeof(XMFLOAT4X4));
    }

    PreparedObjects.clear();
    PreparedObjects.reserve(Scene.Objects.size());
    for (const RenderObject& object : Scene.Objects)
    {
        if (!MeshRegistry::IsLive(object.Mesh))
            continue;
        bool isSkinned = object.SkinnedMesh.IsValid();
        if (isSkinned && !Skinning::IsLive(object.SkinnedMesh))
            continue;

        const MeshRecord& mesh = MeshRegistry::Get(object.Mesh);
        XMMATRIX world = XMLoadFloat4x4(&object.World);

        PreparedObject prepared;
        prepared.Instance.World = object.World;
        XMStoreFloat4x4(&prepared.Instance.Normal, XMMatrixTranspose(XMMatrixInverse(nullptr, world)));

        if (object.Bounds.w > 0.0f)
            prepared.Sphere = object.Bounds;
        else
        {
            XMVECTOR center = XMVector3TransformCoord(XMLoadFloat3(&mesh.BoundsCenter), world);
            float scale = (std::max)({ XMVectorGetX(XMVector3Length(world.r[0])),
                                       XMVectorGetX(XMVector3Length(world.r[1])),
                                       XMVectorGetX(XMVector3Length(world.r[2])) });
            XMStoreFloat4(&prepared.Sphere, XMVectorSetW(center, mesh.BoundsRadius * scale));
        }

        // Batches sort by material type first, so a pass changes pipeline once per type. A skinned
        // object has its own vertices, so its key is unique and it draws alone.
        prepared.MaterialType = MaterialRegistry::IsLive(object.Material) ? MaterialRegistry::GetType(object.Material) : 0;
        uint64_t materialIndex = object.Material.Index & 0xFFFFF;
        uint64_t meshIndex = (isSkinned ? object.SkinnedMesh.Index : object.Mesh.Index) & 0xFFFFF;
        prepared.BatchKey = (static_cast<uint64_t>(prepared.MaterialType) << 61) | (static_cast<uint64_t>(isSkinned) << 60) |
                            (materialIndex << 40) | (meshIndex << 20);
        prepared.Mesh = object.Mesh;
        prepared.Material = object.Material;
        prepared.SkinnedMesh = object.SkinnedMesh;
        if (isSkinned)
            Skinning::Request(object.SkinnedMesh, paletteBase + object.BoneOffset);
        prepared.CastsShadows = object.CastsShadows;
        PreparedObjects.push_back(prepared);
    }
}

void Renderer::DrawFrame()
{
    if (MainWindow->IsResizing() || MainWindow->GetWidth() == 0 || MainWindow->GetHeight() == 0)
        return;

    std::chrono::steady_clock::time_point frameStart = std::chrono::steady_clock::now();
    StatsInProgress = {};
    if (Stats.FrameNumber > 0)
        StatsInProgress.FrameTimeMilliseconds = std::chrono::duration<double, std::milli>(frameStart - LastFrameStart).count();
    LastFrameStart = frameStart;

    if (!Executor->BeginFrame())
        return;
    StatsInProgress.FrameNumber = Executor->GetFrameNumber();
    StatsInProgress.IsVSyncOn = GRAPHICS_SETTINGS.VSync;
    for (const TimerResult& timer : Executor->GetCompletedTimers())
        StatsInProgress.GpuPasses.push_back({ timer.Name, timer.Milliseconds });

    Readback::DeliverCompleted();

    for (EnlistedExternalPass& external : ExternalPasses)
        if (external.Pass.Prepare)
            external.Pass.Prepare();

    PrepareFrameData();
    StatsInProgress.Objects = static_cast<uint32_t>(PreparedObjects.size());

    Graph->Execute(Executor, MainWindow->GetWidth(), MainWindow->GetHeight());

    Executor->EndFrame();
    Stats = std::move(StatsInProgress);
}

void Renderer::WaitForGpu()
{
    Executor->Wait();
}

int Renderer::End()
{
    Executor->Wait();
    Readback::Shutdown();
    MipGenerator::Shutdown();
    Skinning::Shutdown();
    MaterialRegistry::Shutdown();
    MaterialTypes::Clear();
    Graph->Clear();
    delete Graph;
    Graph = nullptr;

    Allocator->FreeDescriptorSet(SceneSet);
    delete SceneLayout;
    SceneLayout = nullptr;

    Executor->EvictBackBuffers();
    delete Allocator;
    delete Executor;
    delete MainWindow;
    return 0;
}

void Renderer::OnResizeEnd(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    std::cout << "Renderer: Resizing to " << width << "x" << height << std::endl;
    WaitForGpu();
    Executor->ResetWindow();
    Graph->Resize(width, height);
    OnResize.Invoke(width, height);
}
