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
    static class PipelineExecutor* Executor;
    static class BufferAllocator* BufferAlloc;
    static class Window* MainWindow;
    static bool IsInitialized;
    static void* CurrentBackBufferView;
    static void* CurrentBackBuffer;
    static std::vector<RHIStructures::PipelineFrameContext> PipelineFrameContexts;
    
    static FrameCallback StartOfFrameCallback;
    static FrameCallback EndOfFrameCallback; 
    
public:
    static void Start(class Window* mainWindow);
    static void DrawFrame();
    static int End();
    static uint32_t CreatePipelineFrameContext(class Pipeline* pipeline, bool isQuad, bool isPresented);
    static void AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw);
    static void AddDescriptorIDToContext(uint32_t contextIndex, uint64_t descriptorID);
    static Window* GetWindow() { return MainWindow; }
    
    static void SetStartOfFrameCallback(FrameCallback callback);
    static void SetRenderCallback(FrameCallback callback);
    static void Wait();
    static void CreatePipelines(std::vector<RHIStructures::PipelineDesc> pipelineDescs, std::vector<std::vector<RHIStructures::DescriptorSetBinding>>
                                vpBindings);
private:
    static void ExecutePipelineContext(uint32_t contextIndex, bool finalContext);

};