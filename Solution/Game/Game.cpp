#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cwchar>
#include <iostream>
#include <string>
#include "Game.h"
#include "imgui.h"
#include "Camera.h"
#include "CameraController.h"
#include "DerpMover.h"
#include "GameTime.h"
#include "Light.h"
#include "MaterialRegistry.h"
#include "MipGenerator.h"
#include "MeshRegistry.h"
#include "Renderer.h"
#include "Window.h"
#include "Geometry/GeometryImport.h"
#include "Input/InputEventSystem.h"
#include "Passes/ComputeTestPass.h"
#include "Passes/DeferredPasses.h"
#include "Scene/SceneExtraction.h"
#include "Scene/SceneNode.h"
#include "UI/DebugOverlay.h"
#include "UI/NoesisUILayer.h"

using namespace DirectX;

static constexpr uint32_t StressObjectCount = 10000;

static Camera* sCamera = nullptr;
static Light* sLight = nullptr;
static DerpMover* sDerpMover = nullptr;
static CameraController* sCameraCon = nullptr;
static std::vector<MaterialHandle> sMaterials;
static DebugOverlay::PanelID sStatsPanel = 0;
static bool sIsAnimationFrozen = false;

static bool IsSwitchPresent(const wchar_t* name)
{
    return std::wcsstr(GetCommandLineW(), name) != nullptr;
}

static void AssignMaterials(SceneNode* node, const std::vector<MaterialHandle>& materials)
{
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
        component->AssignMaterials(materials);
    for (SceneNode* child : node->GetChildren())
        AssignMaterials(child, materials);
}

static void CollectMeshComponents(SceneNode* node, std::vector<MeshComponent*>& out)
{
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
        out.push_back(component);
    for (SceneNode* child : node->GetChildren())
        CollectMeshComponents(child, out);
}

// A square grid of objects that reuse the shell meshes, so draws merge into a few instanced batches.
static void BuildStressScene(SceneNode* templateRoot, const std::vector<MaterialHandle>& materials)
{
    std::vector<MeshComponent*> templates;
    CollectMeshComponents(templateRoot, templates);

    float largestRadius = 0.1f;
    for (MeshComponent* component : templates)
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
            if (MeshRegistry::IsLive(asset->GetMesh(i).GetGPUMesh()))
                largestRadius = (std::max)(largestRadius, MeshRegistry::Get(asset->GetMesh(i).GetGPUMesh()).BoundsRadius);
    }

    uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(StressObjectCount))));
    float spacing = largestRadius * 2.5f;
    float half = (side - 1) * spacing * 0.5f;

    SceneNode* root = new SceneNode("Stress", XMMatrixIdentity());
    for (uint32_t i = 0; i < StressObjectCount; i++)
    {
        XMMATRIX local = XMMatrixRotationY(i * 0.37f) * XMMatrixTranslation((i % side) * spacing - half, 0.0f, (i / side) * spacing - half);
        SceneNode* node = new SceneNode("Stress " + std::to_string(i), local, root);
        MeshComponent* component = static_cast<MeshComponent*>(node->AddComponent(MeshComponentType));
        component->SetMeshAsset(templates[i % templates.size()]->GetMeshAsset());
        component->AssignMaterials(materials);
    }

    sCamera->SetPositionFloat3({ 0.0f, half * 0.5f, -half * 1.1f });
    sCamera->LookAtFloat3({ 0.0f, 0.0f, 0.0f });
}

static double GpuMilliseconds(const FrameStats& stats, const std::string& passName)
{
    for (const GpuPassTime& time : stats.GpuPasses)
        if (time.Name == passName)
            return time.Milliseconds;
    return 0.0;
}

static void DrawRendererPanel()
{
    const FrameStats& stats = Renderer::GetFrameStats();
    ImGui::Text("Objects %u, vsync %s, mip queue %u", stats.Objects, stats.IsVSyncOn ? "on" : "off", MipGenerator::GetQueueLength());
    if (ImGui::BeginTable("##Passes", 5, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Instances");
        ImGui::TableSetupColumn("Batches");
        ImGui::TableSetupColumn("Draws");
        ImGui::TableSetupColumn("GPU ms");
        ImGui::TableHeadersRow();
        for (const PassStats& pass : stats.Passes)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(pass.Name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%u", pass.Instances);
            ImGui::TableNextColumn(); ImGui::Text("%u", pass.Batches);
            ImGui::TableNextColumn(); ImGui::Text("%u", pass.DrawCalls);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", GpuMilliseconds(stats, pass.Name));
        }
        for (const GpuPassTime& time : stats.GpuPasses)
        {
            bool isListed = false;
            for (const PassStats& pass : stats.Passes)
                isListed |= pass.Name == time.Name;
            if (isListed)
                continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(time.Name.c_str());
            ImGui::TableNextColumn(); ImGui::TableNextColumn(); ImGui::TableNextColumn();
            ImGui::TableNextColumn(); ImGui::Text("%.3f", time.Milliseconds);
        }
        ImGui::EndTable();
    }
}

void GameInit()
{
    NoesisUILayer::Instance().NoesisInit();
    DeferredPasses::Enlist();
    if (IsSwitchPresent(L" -compute-test"))
        ComputeTestPass::Enlist();

    sCamera = new Camera(90, 1280.0f / 720.0f, { 0, 10, -8 });
    Camera::ActiveCamera = sCamera;
    sCamera->LookAtFloat3({});

    RHIStructures::LightData lightData = {};
    lightData.Position = XMFLOAT3(-20.0f, 20.0f, -20.0f);
    lightData.Colour = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lightData.Intensity = 20.0f;
    lightData.Radius = 50.0f;
    lightData.Type = 1;
    sLight = new Light(lightData, true);

    std::vector<MaterialHandle> shellMaterials = { MaterialRegistry::Enlist("shells_0"), MaterialRegistry::Enlist("shells_1") };
    std::vector<MaterialHandle> derpMaterials = { MaterialRegistry::Enlist("derp_0") };
    sMaterials = shellMaterials;
    sMaterials.insert(sMaterials.end(), derpMaterials.begin(), derpMaterials.end());

    SceneNode* shells = GeometryImport::CreateMeshGroup("../Engine/Meshes/shells.fbx", "Shells", XMMatrixIdentity(), false);
    AssignMaterials(shells, shellMaterials);
    shells->UploadToGPU();

    SceneNode* derp = GeometryImport::CreateMeshGroup("../Engine/Meshes/Derp.fbx", "derp", XMMatrixIdentity(), true);
    AssignMaterials(derp, derpMaterials);
    derp->UploadToGPU();
    sDerpMover = new DerpMover(derp);

    if (IsSwitchPresent(L" -stress"))
        BuildStressScene(shells, shellMaterials);

    sCameraCon = new CameraController(sCamera, 5.0f);
    sIsAnimationFrozen = IsSwitchPresent(L" -freeze-animation");
    sStatsPanel = DebugOverlay::EnlistPanel("Renderer", DrawRendererPanel);
    GameTime::Init();
}

void GameRunFrame(float /*dt*/)
{
    GameTime::UpdateTime();
    InputEventSystem::PollInput(Renderer::GetWindow()->GetWindowHandle(), GameTime::GetDeltaTime());
    InputEventSystem::ProcessCommands(GameTime::GetDeltaTime());
    NoesisUILayer::Instance().NoesisUpdate(GameTime::GetDeltaTime());
    if (sDerpMover) sDerpMover->MoveTheDerp(sIsAnimationFrozen ? 0.0 : GameTime::GetDeltaTime());
    if (sCameraCon) sCameraCon->MoveCamera(GameTime::GetDeltaTime());
    SceneExtraction::ExtractFrame(sCamera, { sLight });
}

void GameShutdown()
{
    const FrameStats& stats = Renderer::GetFrameStats();
    std::cout << "Last frame: " << stats.Objects << " objects, " << stats.DrawCalls << " draw calls, "
              << stats.FrameTimeMilliseconds << " ms, " << MipGenerator::GetGeneratedCount() << " mip chains generated" << std::endl;
    for (const PassStats& pass : stats.Passes)
        std::cout << "  " << pass.Name << ": " << pass.Instances << " instances, " << pass.Batches << " batches, "
                  << pass.DrawCalls << " draws" << std::endl;
    std::cout << "  GPU ms:";
    for (const GpuPassTime& time : stats.GpuPasses)
        std::cout << " " << time.Name << " " << time.Milliseconds << ";";
    std::cout << std::endl;

    Renderer::WaitForGpu();
    DebugOverlay::RetirePanel(sStatsPanel);
    ComputeTestPass::Retire();
    DeferredPasses::Retire();
    Renderer::GetRenderScene().Clear();

    for (SceneNode* root : SceneNode::GetRootNodes())
        root->FreeGPUResources();
    SceneNode::DestroyAll();

    for (MaterialHandle material : sMaterials)
        MaterialRegistry::Retire(material);
    sMaterials.clear();

    NoesisUILayer::Instance().NoesisShutdown();

    delete sDerpMover; sDerpMover = nullptr;
    delete sCameraCon; sCameraCon = nullptr;
    delete sCamera;    sCamera = nullptr;
    delete sLight;     sLight = nullptr;
}
