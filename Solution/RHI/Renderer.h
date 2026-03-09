#pragma once
#include <vector>
#include "../../Common/RHI/RHIStructures.h"
#include "RHI_API_Macro.h"


class RHI_API Renderer
{
public:
    static void Start(class Window* mainWindow);
    static int Run();
    static int End();
    static uint32_t CreatePipelineFrameContext(class Pipeline* pipeline, bool isQuad);
    static void AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw);
    
private:
    static class PipelineExecutor* executor;
    static class BufferAllocator* bufferAlloc;
    static class Window* window;
    static bool initialized;
    
    static std::vector<RHIStructures::PipelineFrameContext> PipelineFrameContexts;
    
    // temp
    static class Pipeline* PBRGeometryPipe;
    static class Pipeline* PBRLightingPipe;
};