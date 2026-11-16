#pragma once
#include <functional>
#include <string>
#include <vector>
#include "RHI/RHIStructures.h"
#include "RENDERER_API_Macro.h"
#include "Data/Event.h"
#include "RenderScene.h"

class Window;
class PipelineExecutor;
class PipelineLayout;
class PassGraph;
struct PreparedObject;

// A pass recorded by code outside the renderer, such as a UI library, using its own native calls.
// Prepare runs before the frame's passes, outside any render pass. Render runs inside an overlay
// pass of the pass graph, after every raster pass.
struct ExternalPass
{
    std::function<void()> Prepare;
    std::function<void()> Render;
    int Order = 0;
    std::string Name = "External";
};

struct PassStats
{
    std::string Name;
    uint32_t Instances = 0;
    uint32_t Batches = 0;
    uint32_t DrawCalls = 0;
};

struct GpuPassTime
{
    std::string Name;
    double Milliseconds = 0.0;
};

struct FrameStats
{
    uint64_t FrameNumber = 0;
    double FrameTimeMilliseconds = 0.0;
    uint32_t Objects = 0;
    uint32_t DrawCalls = 0;
    std::vector<PassStats> Passes;
    std::vector<GpuPassTime> GpuPasses;     // From a frame a few frames earlier; see PipelineExecutor timers
    bool IsVSyncOn = true;
};

// The scene set (set 1) holds the frame constants and the instance buffer; the
// material set (set 2) holds a material's parameters and textures (see MaterialRegistry). Both belong
// to GetSceneLayout. Passes with a different layout can still bind the scene set if their set 1 is
// GetSceneSetBindings().
class RENDERER_API Renderer
{
public:
    using ExternalPassID = size_t;
    static constexpr uint32_t SceneSetIndex = 1;

    static void Start(Window* mainWindow);
    static void DrawFrame();
    static int End();
    static void WaitForGpu();

    static Window* GetWindow();
    static Event<uint32_t, uint32_t>& EventOnResize();
    static const FrameStats& GetFrameStats();

    static ExternalPassID EnlistExternalPass(ExternalPass pass);
    static void RetireExternalPass(ExternalPassID id);

    static void SetGameViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    static void GetGameViewport(uint32_t& x, uint32_t& y, uint32_t& w, uint32_t& h);

    static PassGraph& GetPassGraph();
    static RenderScene& GetRenderScene();
    static PipelineLayout* GetSceneLayout();
    static std::vector<RHIStructures::DescriptorBinding> GetSceneSetBindings();
    static RHIStructures::ShaderStageMask GetSceneVisibleStages();
    static void BindSceneSet(PipelineExecutor* executor);
    static PipelineExecutor* GetExecutor();
    static RHIStructures::Format GetBackBufferFormat();

    static const std::vector<PreparedObject>& GetPreparedObjects();
    static void ReportPassStats(const std::string& name, uint32_t instances, uint32_t batches, uint32_t drawCalls);

    static void OnResizeEnd(uint32_t width, uint32_t height);
};
