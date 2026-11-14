#include "DebugOverlay.h"

#include <vector>
#include "ImGuiLayer.h"
#include "imgui.h"
#include "ForwardInterface.h"
#include "Renderer.h"
#include "Window.h"

struct EnlistedPanel
{
    DebugOverlay::PanelID ID;
    std::string Title;
    DebugOverlay::PanelCallback Callback;
};

static constexpr float WindowMargin = 10.0f;
static constexpr double FrameTimeSmoothing = 0.05;

static std::vector<EnlistedPanel> Panels;
static DebugOverlay::PanelID NextPanelID = 0;
static ImGuiLayer::Subscription DrawSubscription = 0;
static bool IsOverlayVisible = true;
static bool HasStarted = false;
static double SmoothedFrameTime = 0.0;

static void Draw()
{
    if (!IsOverlayVisible)
        return;

    const FrameStats& stats = Renderer::GetFrameStats();
    SmoothedFrameTime = SmoothedFrameTime == 0.0
        ? stats.FrameTimeMilliseconds
        : SmoothedFrameTime + (stats.FrameTimeMilliseconds - SmoothedFrameTime) * FrameTimeSmoothing;

    uint32_t viewX, viewY, viewWidth, viewHeight;
    Renderer::GetGameViewport(viewX, viewY, viewWidth, viewHeight);
    if (viewWidth == 0)
        viewWidth = Renderer::GetWindow()->GetWidth();

    ImGui::SetNextWindowPos(ImVec2(viewX + viewWidth - WindowMargin, viewY + WindowMargin), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Debug", nullptr, flags))
    {
        ImGui::TextUnformatted(ForwardInterface::GetCurrentAPI() == Vulkan ? "Vulkan" : "Direct3D 12");
        ImGui::Text("Frame %.2f ms (%.0f fps)", SmoothedFrameTime, SmoothedFrameTime > 0.0 ? 1000.0 / SmoothedFrameTime : 0.0);
        ImGui::Text("Draw calls %u", stats.DrawCalls);

        for (size_t i = 0; i < Panels.size(); i++)
            if (ImGui::CollapsingHeader(Panels[i].Title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                Panels[i].Callback();
    }
    ImGui::End();
}

void DebugOverlay::Start()
{
    if (HasStarted)
        return;

    DrawSubscription = ImGuiLayer::Subscribe(Draw);
    HasStarted = true;
}

void DebugOverlay::Shutdown()
{
    if (!HasStarted)
        return;

    ImGuiLayer::Unsubscribe(DrawSubscription);
    Panels.clear();
    HasStarted = false;
}

void DebugOverlay::SetVisible(bool isVisible)
{
    IsOverlayVisible = isVisible;
}

DebugOverlay::PanelID DebugOverlay::EnlistPanel(const std::string& title, PanelCallback callback)
{
    PanelID id = NextPanelID++;
    Panels.push_back({ id, title, std::move(callback) });
    return id;
}

void DebugOverlay::RetirePanel(PanelID panel)
{
    std::erase_if(Panels, [panel](const EnlistedPanel& enlisted) { return enlisted.ID == panel; });
}
