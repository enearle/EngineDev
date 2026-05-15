#pragma once
#include <functional>
#include <vector>
#include "RHI/RHIStructures.h"
#include "RHI_API_Macro.h"
#include "Data/Event.h"

class RHI_API Renderer
{
public:
    using RCallback = std::function<void()>;
    
private:
    static class PipelineExecutor* Executor;
    static class BufferAllocator* BufferAlloc;
    static class Window* MainWindow;
    static bool IsInitialized;
    static void* CurrentBackBufferView;
    static void* CurrentBackBuffer;
    static std::vector<RHIStructures::PipelineFrameContext> PipelineFrameContexts;
    
    static Event<> OnStartOfFrame;
    static Event<> OnEndOfFrame;
    static Event<uint32_t, uint32_t> OnResize;
    
public:
    
    static void Start(class Window* mainWindow);
    static void DrawFrame();
    static int End();
    static uint32_t CreatePipelineFrameContext(class Pipeline* pipeline, bool isQuad, bool isPresented);
    static void AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw);
    static void AddDescriptorIDToContext(uint32_t contextIndex, uint64_t descriptorID);
    static Window* GetWindow() { return MainWindow; }
    
    static Event<>& EventOnStartOfFrame() { return OnStartOfFrame; }
    static Event<>& EventOnEndOfFrame() { return OnEndOfFrame; }
    static Event<uint32_t, uint32_t>& EventOnResize() { return OnResize; }
    
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
