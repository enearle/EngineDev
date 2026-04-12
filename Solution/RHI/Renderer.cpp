#include "Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/PipelineExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/RHI/Uniform.h"
#include "../../Common/RHI/BufferAllocator.h"

using namespace RHIConstants;

PipelineExecutor* Renderer::executor = nullptr;
BufferAllocator* Renderer::bufferAlloc = nullptr;
Window* Renderer::window = nullptr;
bool Renderer::initialized = false;
std::vector<PipelineFrameContext> Renderer::PipelineFrameContexts;
void* Renderer::CurrentBackBuffer = nullptr;
void* Renderer::CurrentBackBufferView = nullptr;
Renderer::FrameCallback Renderer::startOfFrameCallback = nullptr;
Renderer::FrameCallback Renderer::renderCallback = nullptr;

void Renderer::Start(Window* mainWindow)
{
    window = mainWindow;
    
    CoreInitData data;
    data.SwapchainMSAA = false;
    data.SwapchainMSAASamples = 1;
    
    executor = PipelineExecutor::Create(window, data);
    bufferAlloc = BufferAllocator::GetInstance();
    
    ShowWindow(window->GetWindowHandle(), 5);
}

void Renderer::DrawFrame()
{
    // Start of Frame
    executor->BeginFrame();
    executor->GetSwapChainRenderTargets(CurrentBackBufferView, CurrentBackBuffer);
    ImageMemoryBarrier preBarrier = PRE_BARRIER;
    preBarrier.ImageResource = CurrentBackBuffer;
    executor->IssueImageMemoryBarrier(preBarrier);
    
    // Call Noesis Pre-Frame Callback
    if (startOfFrameCallback) startOfFrameCallback();
    
    // Load/unload time
    if (!initialized)
    {
        ImageMemoryBarrier initBarrier = INIT_BARRIER;
        for (int i = 0; i < PipelineFrameContexts.size(); i++)
            for (int j = 0; j < PipelineFrameContexts[i].ContextPipeline->GetOwnedImageCount(); j++)
            {
                initBarrier.ImageResource = PipelineFrameContexts[i].ContextPipeline->GetOwnedImage(j);
                executor->IssueImageMemoryBarrier(initBarrier);
            }
        
        initialized = true;
    }
    
    for (uint32_t i = 0; i < PipelineFrameContexts.size(); i++)
        ExecutePipelineContext(i, i == PipelineFrameContexts.size() - 1);
    
    // End of Frame
    ImageMemoryBarrier postBarrier = POST_BARRIER;
    postBarrier.ImageResource = CurrentBackBuffer;
    executor->IssueImageMemoryBarrier(postBarrier);
    executor->EndFrame();
}

int Renderer::End()
{
    executor->Wait();

    for (auto context : PipelineFrameContexts)
    {
        delete context.ContextPipeline;
        context.ContextPipeline = nullptr;
    }
    
    delete bufferAlloc;
    delete executor;
    delete window;

    return 0;
}

uint32_t Renderer::CreatePipelineFrameContext(Pipeline* pipeline, bool isQuad, bool isPresented)
{
    PipelineFrameContext context;
    context.ContextPipeline = pipeline;
    context.IsFSQuad = isQuad;
    context.IsPresented = isPresented;
    PipelineFrameContexts.push_back(context);
    return PipelineFrameContexts.size() - 1;
}

void Renderer::AddIndexedDrawToContext(uint32_t contextIndex, IndexedDraw draw)
{
    PipelineFrameContexts[contextIndex].IndexedDraws.push_back(draw);
}

void Renderer::AddDescriptorIDToContext(uint32_t contextIndex, uint64_t descriptorID)
{
    PipelineFrameContexts[contextIndex].PerFrameDescriptors.push_back(descriptorID);
}

void Renderer::ExecutePipelineContext(uint32_t contextIndex, bool finalContext)
{
    // Pre-draw attachment barrier
    ImageMemoryBarrier readToAttachmentBarrier = READ_TO_ATTACHMENT_BARRIER;
    for (int j = 0; j < PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImageCount(); j++)
    {
        readToAttachmentBarrier.ImageResource = PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImage(j);
        executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
    }
    
    // Begin Pipeline
    if (PipelineFrameContexts[contextIndex].IsPresented)
        executor->BeginPipeline(PipelineFrameContexts[contextIndex].ContextPipeline, {CurrentBackBufferView}, nullptr, window->GetWidth(), window->GetHeight());
    else
        executor->BeginPipeline(PipelineFrameContexts[contextIndex].ContextPipeline, {}, nullptr, window->GetWidth(), window->GetHeight());

    // Bind Descriptors and Draw
    std::vector<uint64_t>* perFrameDescriptors = PipelineFrameContexts[contextIndex].PerFrameDescriptors.size() > 0 
        ? &PipelineFrameContexts[contextIndex].PerFrameDescriptors : nullptr;
    uint32_t numFramDescs = perFrameDescriptors ? perFrameDescriptors->size() : 0;
    
    executor->BindPipelineDescriptorSets(perFrameDescriptors);
    
    if (PipelineFrameContexts[contextIndex].IsFSQuad)
        executor->DrawFSQuad();
    else
        for (int j = 0; j < PipelineFrameContexts[contextIndex].IndexedDraws.size(); j++)
        {
            executor->BindDrawDescriptorSets(&PipelineFrameContexts[contextIndex].IndexedDraws[j].PerDrawDescriptors, numFramDescs);
            executor->DrawIndexed(
                PipelineFrameContexts[contextIndex].IndexedDraws[j].VertexBufferID,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].VertexCount,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].IndexBufferID,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].IndexCount,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].PushConstants,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].PushConstantSize
            );
        }
    
    // Noesis Render callback
    if (finalContext && renderCallback) renderCallback();
    
    // End Pipeline
    executor->EndPipeline();
    
    // Post-draw attachment barrier
    ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
    for (int j = 0; j < PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImageCount(); j++)
    {
        gBufferBarrier.ImageResource = PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImage(j);
        executor->IssueImageMemoryBarrier(gBufferBarrier);
    }
}

void Renderer::SetStartOfFrameCallback(FrameCallback callback)
{
    startOfFrameCallback = callback;
}

void Renderer::SetRenderCallback(FrameCallback callback)
{
    renderCallback = callback;
}
