#pragma once
#include <functional>
#include <vector>
#include "RHI/RHIStructures.h"
#include "RENDERER_API_Macro.h"
#include "Data/Event.h"

class Window;
class Pipeline;
class PipelineExecutor;
class BufferAllocator;

// A pass recorded by code outside the renderer, such as a UI library, using its own native calls.
// Prepare runs before the frame's passes, outside any render pass. Render runs inside the overlay pass.
struct ExternalPass
{
    std::function<void()> Prepare;
    std::function<void()> Render;
    int Order = 0;
};

struct FrameStats
{
    uint64_t FrameNumber = 0;
    double FrameTimeMilliseconds = 0.0;
    uint32_t DrawCalls = 0;
};

class RENDERER_API Renderer
{
public:
    using ExternalPassID = size_t;

private:
    static PipelineExecutor* Executor;
    static BufferAllocator* BufferAlloc;
    static Window* MainWindow;
    static bool IsInitialized;
    static void* CurrentBackBufferView;
    static void* CurrentBackBuffer;
    static std::vector<RHIStructures::PipelineFrameContext> PipelineFrameContexts;
    static Event<uint32_t, uint32_t> OnResize;
    static FrameStats Stats;

public:

    static void Start(Window* mainWindow);
    static void DrawFrame();
    static int End();
    static uint32_t CreatePipelineFrameContext(Pipeline* pipeline, bool isQuad, bool isPresented);
    static void AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw,
        const std::vector<RHIStructures::DescriptorSetHandle>& perMaterialDescriptors = {});
    static void AddDescriptorIDToContext(uint32_t contextIndex, RHIStructures::DescriptorSetHandle descriptorID);
    static Window* GetWindow() { return MainWindow; }
    static Event<uint32_t, uint32_t>& EventOnResize() { return OnResize; }
    static const FrameStats& GetFrameStats() { return Stats; }

    static ExternalPassID EnlistExternalPass(ExternalPass pass);
    static void RetireExternalPass(ExternalPassID id);

    static void SetGameViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    static void GetGameViewport(uint32_t& x, uint32_t& y, uint32_t& w, uint32_t& h);

    static void WaitForGpu();
    static void ResetPipelines();
    static void CreatePipelines(std::vector<RHIStructures::PipelineDesc> pipelineDescs,
        std::vector<std::vector<RHIStructures::PipelineDescriptorData>> pipelineDescriptors);

    static void OnResizeEnd(uint32_t width, uint32_t height);
private:
    static void ExecutePipelineContext(uint32_t contextIndex, bool finalContext);

};
