#pragma once
#include <functional>
#include <vector>
#include "../../Common/RHI/RHIStructures.h"
#include "RHI_API_Macro.h"

class RHI_API Renderer
{
public:
    using FrameCallback = std::function<void()>;
    
private:
    static class PipelineExecutor* executor;
    static class BufferAllocator* bufferAlloc;
    static class Window* window;
    static bool initialized;
    static void* CurrentBackBufferView;
    static void* CurrentBackBuffer;
    static std::vector<RHIStructures::PipelineFrameContext> PipelineFrameContexts;
    
    static FrameCallback startOfFrameCallback;
    static FrameCallback renderCallback; 
    
public:
    static void Start(class Window* mainWindow);
    static void DrawFrame();
    static int End();
    static uint32_t CreatePipelineFrameContext(class Pipeline* pipeline, bool isQuad, bool isPresented);
    static void AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw);
    static void AddDescriptorIDToContext(uint32_t contextIndex, uint64_t descriptorID);
    static Window* GetWindow() { return window; }
    
    static void SetStartOfFrameCallback(FrameCallback callback);
    static void SetRenderCallback(FrameCallback callback);
    
private:
    static void ExecutePipelineContext(uint32_t contextIndex, bool finalContext);

};