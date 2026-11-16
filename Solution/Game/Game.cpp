#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <string>
#include "Game.h"
#include "AnimationDemo.h"
#include "PagingTest.h"
#include "Paging.h"
#include "imgui.h"
#include "Camera.h"
#include "CameraController.h"
#include "GameTime.h"
#include "Light.h"
#include "MaterialRegistry.h"
#include "MipGenerator.h"
#include "MeshRegistry.h"
#include "Renderer.h"
#include "Window.h"
#include "Animation/AnimationSystem.h"
#include "Animation/AnimatorComponent.h"
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
static CameraController* sCameraCon = nullptr;
static std::vector<MaterialHandle> sMaterials;
static DebugOverlay::PanelID sStatsPanel = 0;
static DebugOverlay::PanelID sPagingPanel = 0;

static bool IsSwitchPresent(const wchar_t* name)
{
    return std::wcsstr(GetCommandLineW(), name) != nullptr;
}

// The number after a switch, or fallback when the switch has none.
static uint32_t SwitchNumber(const wchar_t* name, uint32_t fallback)
{
    const wchar_t* found = std::wcsstr(GetCommandLineW(), name);
    if (!found)
        return 0;
    wchar_t* end = nullptr;
    unsigned long value = std::wcstoul(found + std::wcslen(name), &end, 10);
    return end == found + std::wcslen(name) ? fallback : static_cast<uint32_t>(value);
}

// The text after a switch up to the next space, or fallback when the switch is absent.
static std::wstring SwitchText(const wchar_t* name, const std::wstring& fallback)
{
    const wchar_t* found = std::wcsstr(GetCommandLineW(), name);
    if (!found)
        return fallback;
    const wchar_t* start = found + std::wcslen(name);
    while (*start == L' ')
        start++;
    const wchar_t* end = start;
    while (*end && *end != L' ')
        end++;
    return std::wstring(start, end);
}

static float WorldRadius(SceneNode* character)
{
    AnimatorComponent* animator = static_cast<AnimatorComponent*>(character->GetComponent(AnimatorComponentType));
    return animator ? animator->GetModelBounds().w * XMVectorGetX(XMVector3Length(character->GetWorldMatrix().r[0])) : 0.0f;
}

// Files arrive in their own units, so the character is scaled from its joint bounds to the Derp's
// size, whatever its proportions, and takes the Derp's place in the middle of the view.
static void PlaceCharacter(SceneNode* character, SceneNode* derp)
{
    float derpRadius = WorldRadius(derp);
    float radius = WorldRadius(character);
    float scale = radius > 0.0f && derpRadius > 0.0f ? derpRadius / radius : 1.0f;
    character->SetLocalMatrix(character->GetLocalMatrix() * XMMatrixScaling(scale, scale, scale));
    derp->SetLocalMatrix(derp->GetLocalMatrix() * XMMatrixTranslation(-2.0f * derpRadius, 0.0f, 0.0f));
}

static void AssignMaterials(SceneNode* node, const std::vector<MaterialHandle>& materials)
{
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
        component->AssignMaterials(materials);
    for (SceneNode* child : node->GetChildren())
        AssignMaterials(child, materials);
}

static void AssignMaterialsByName(SceneNode* node)
{
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
        component->AssignMaterialsByName();
    for (SceneNode* child : node->GetChildren())
        AssignMaterialsByName(child);
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

// The shells, the Derp, the character and its crowd, and the stress grid.
static void BuildDemoScene()
{
    std::vector<MaterialHandle> shellMaterials = { MaterialRegistry::Enlist("shells_0"), MaterialRegistry::Enlist("shells_1") };
    std::vector<MaterialHandle> derpMaterials = { MaterialRegistry::Enlist("derp_0") };
    sMaterials = shellMaterials;
    sMaterials.insert(sMaterials.end(), derpMaterials.begin(), derpMaterials.end());

    SceneNode* shells = GeometryImport::CreateMeshGroup("../Engine/Meshes/shells.fbx", "Shells", XMMatrixIdentity(), false);
    AssignMaterials(shells, shellMaterials);
    shells->UploadToGPU();

    SceneNode* derp = GeometryImport::CreateMeshGroup("../Engine/Meshes/Derp.fbx", "derp", XMMatrixIdentity(), true);
    AssignMaterials(derp, derpMaterials);
    std::vector<SceneNode*> characters = { derp };

    // The character lives outside the repository, since its licence forbids publishing it. Without
    // it the Derp stands in. -animation-test prefers the export that carries two clips.
    std::wstring defaultCharacter = L"../../TempAssets/CharBlend.fbx";
    if (IsSwitchPresent(L" -animation-test") && std::filesystem::exists(L"../../TempAssets/CharBlendClips.fbx"))
        defaultCharacter = L"../../TempAssets/CharBlendClips.fbx";
    std::filesystem::path characterPath = SwitchText(L" -character", defaultCharacter);
    if (std::filesystem::exists(characterPath))
    {
        try
        {
            SceneNode* character = GeometryImport::CreateMeshGroup(characterPath.string(), "character", XMMatrixIdentity(), true);
            PlaceCharacter(character, derp);
            AssignMaterialsByName(character);
            characters.push_back(character);
        }
        catch (const std::exception& error)
        {
            std::cout << "Character not loaded, the Derp stands in: " << error.what() << std::endl;
        }
    }

    // The demo copies source vertices for the skinning check before the upload drops them.
    AnimationDemo::Start(characters, derpMaterials, SwitchNumber(L" -crowd", 100), IsSwitchPresent(L" -animation-test"));
    for (SceneNode* character : characters)
        character->UploadToGPU();

    if (IsSwitchPresent(L" -stress"))
        BuildStressScene(shells, shellMaterials);
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
    if (IsSwitchPresent(L" -paging-test"))
    {
        lightData.Position = XMFLOAT3(0.0f, 30.0f, 0.0f);
        lightData.Intensity = 4.0f;
        lightData.Radius = 120.0f;
    }
    sLight = new Light(lightData, true);

    if (IsSwitchPresent(L" -paging-test"))
        PagingTest::Start(std::filesystem::path(SwitchText(L" -paging-image", L"")).string());
    else
        BuildDemoScene();
    Paging::SetTint(IsSwitchPresent(L" -paging-tint"));

    sCameraCon = new CameraController(sCamera, 5.0f);
    AnimationSystem::SetFrozen(IsSwitchPresent(L" -freeze-animation"));
    sStatsPanel = DebugOverlay::EnlistPanel("Renderer", DrawRendererPanel);
    sPagingPanel = DebugOverlay::EnlistPanel("Paging", PagingTest::DrawPanel);
    GameTime::Init();
}

void GameRunFrame(float /*dt*/)
{
    GameTime::UpdateTime();
    InputEventSystem::PollInput(Renderer::GetWindow()->GetWindowHandle(), GameTime::GetDeltaTime());
    InputEventSystem::ProcessCommands(GameTime::GetDeltaTime());
    NoesisUILayer::Instance().NoesisUpdate(GameTime::GetDeltaTime());
    if (sCameraCon) sCameraCon->MoveCamera(GameTime::GetDeltaTime());
    PagingTest::Update(static_cast<float>(GameTime::GetDeltaTime()), sCamera);
    AnimationDemo::Update(static_cast<float>(GameTime::GetDeltaTime()));
    AnimationSystem::Update(static_cast<float>(GameTime::GetDeltaTime()));
    SceneExtraction::ExtractFrame(sCamera, { sLight });
    AnimationDemo::RequestChecks();
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
    AnimationDemo::PrintResults();
    PagingTest::PrintResults();

    Renderer::WaitForGpu();
    DebugOverlay::RetirePanel(sStatsPanel);
    DebugOverlay::RetirePanel(sPagingPanel);
    ComputeTestPass::Retire();
    DeferredPasses::Retire();
    Renderer::GetRenderScene().Clear();

    for (SceneNode* root : SceneNode::GetRootNodes())
        root->FreeGPUResources();
    SceneNode::DestroyAll();

    for (MaterialHandle material : sMaterials)
        MaterialRegistry::Retire(material);
    sMaterials.clear();

    AnimationDemo::Shutdown();
    PagingTest::Shutdown();
    NoesisUILayer::Instance().NoesisShutdown();

    delete sCameraCon; sCameraCon = nullptr;
    delete sCamera;    sCamera = nullptr;
    delete sLight;     sLight = nullptr;
}
