#define WIN32_LEAN_AND_MEAN
#define IN_EDITOR
#include <windows.h>
#include "GraphicsSettings.h"
#include "imgui.h"
#include "Renderer.h"
#include "Window.h"
#include "UI/ImGuiLayer.h"
#include "UI/DebugOverlay.h"
#include "../Game/Game.h"
#include "FileExplorer.h"
#include "SceneExplorer.h"
#include "Modals/FileMove.h"
#include "Modals/Importer.h"
#include "Modals/NewDirectory.h"
#include "Resources/ResourceManager.h"

static bool IS_PLAYING      = false;
static bool SHOULD_START    = false;
static bool SHOULD_STOP     = false;

static void DrawEditorInterface()
{
    // Full-screen host panel (no title bar, no padding)
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize    | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar    |
        ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##Host", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    // Menu bar with Play / Stop controls
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
            ImGui::EndMenu();
        }

        float btnOffset = ImGui::GetContentRegionAvail().x * 0.5f - 60.f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnOffset);

        ImGui::BeginDisabled(IS_PLAYING);
        if (ImGui::Button(" Play "))
        {
            IS_PLAYING   = true;
            SHOULD_START = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!IS_PLAYING);
        if (ImGui::Button(" Stop "))
        {
            IS_PLAYING  = false;
            SHOULD_STOP = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextUnformatted(IS_PLAYING ? "Playing" : "Stopped");

        ImGui::EndMenuBar();
    }

    // Left sidebar: Scene Hierarchy + Properties
    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;
    float sideW  = 220.f * ImGui::GetStyle().FontScaleDpi;

    ImGui::BeginChild("##LeftPanel", ImVec2(sideW, totalH), true);
    float halfH = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y * 4.f) * 0.5f;
    ImGui::TextUnformatted("Scene Hierarchy");
    ImGui::Separator();
    ImGui::BeginChild("##SceneTreeHost", ImVec2(0, halfH), false);
    SceneExplorer::ShowSceneTree();
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::TextUnformatted("Files");
    ImGui::Separator();
    ImGui::BeginChild("##FileTreeHost", ImVec2(0, 0), false);
    FileExplorer::ShowFileTree("../Game/Assets");
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::SameLine();

    // Game viewport panel — record its screen rect so the renderer can restrict the final pass
    ImGui::BeginChild("##GameViewport", ImVec2(totalW - sideW, totalH), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 vpPos  = ImGui::GetWindowPos();
        ImVec2 vpSize = ImGui::GetWindowSize();
        Renderer::SetGameViewport(
            static_cast<uint32_t>(vpPos.x),  static_cast<uint32_t>(vpPos.y),
            static_cast<uint32_t>(vpSize.x), static_cast<uint32_t>(vpSize.y));
    }
    ImGui::EndChild();

    NewDirectory::GetInstance().Render();
    FileMove::GetInstance().Render();
    Importer::GetInstance().Render();

    ImGui::End();
}

int main(int argumentCount, char** arguments)
{
    GRAPHICS_SETTINGS.SetEditorMode(true);
    GRAPHICS_SETTINGS.ApplyCommandLine(argumentCount, arguments);
    ResourceManager::LoadRegistry();

    for (int i = 1; i < argumentCount; i++)
        if (strcmp(arguments[i], "-play") == 0)
            IS_PLAYING = SHOULD_START = true;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Window* window = new Window(L"Engine Editor", Win32, 1600, 900);
    Renderer::Start(window);

    ImGuiLayer::Start(true);
    ImGuiLayer::Subscribe(DrawEditorInterface);
    DebugOverlay::Start();

    while (!window->PeekMessages())
    {
        if (SHOULD_START) { GameInit();     SHOULD_START = false; }
        if (SHOULD_STOP)  { GameShutdown(); SHOULD_STOP  = false; }
        if (IS_PLAYING)
            GameRunFrame(0.0f);
        Renderer::DrawFrame();
    }

    if (IS_PLAYING)
    {
        IS_PLAYING = false;
        GameShutdown();
    }

    Renderer::WaitForGpu();
    DebugOverlay::Shutdown();
    ImGuiLayer::Shutdown();

    return Renderer::End();
}
