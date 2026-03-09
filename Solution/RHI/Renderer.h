#pragma once
#include "RHI_API_Macro.h"


class RHI_API Renderer
{
public:
    static void Start(class Window* mainWindow);
    static int Run();
    static int End();
    
private:
    static class PipelineExecutor* executor;
    static class BufferAllocator* bufferAlloc;
    static class Window* window;
    static bool initialized;
    
    
    // temp
    static class Pipeline* PBRGeometryPipe;
    static class Pipeline* PBRLightingPipe;
};