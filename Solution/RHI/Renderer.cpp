#include "Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/PipelineExecutor.h"
#include <DirectXMath.h>
#include "../../Common/RHI/BufferAllocator.h"

using namespace RHIConstants;

PipelineExecutor* Renderer::Executor = nullptr;
BufferAllocator* Renderer::BufferAlloc = nullptr;
Window* Renderer::MainWindow = nullptr;
bool Renderer::IsInitialized = false;
std::vector<PipelineFrameContext> Renderer::PipelineFrameContexts;
void* Renderer::CurrentBackBuffer = nullptr;
void* Renderer::CurrentBackBufferView = nullptr;
Renderer::FrameCallback Renderer::StartOfFrameCallback = nullptr;
Renderer::FrameCallback Renderer::EndOfFrameCallback = nullptr;

void Renderer::Start(Window* mainWindow)
{
    MainWindow = mainWindow;
    
    CoreInitData data;
    data.SwapchainMSAA = false;
    data.SwapchainMSAASamples = 1;
    
    Executor = PipelineExecutor::Create(MainWindow, data);
    BufferAlloc = BufferAllocator::GetInstance();
    
    ShowWindow(MainWindow->GetWindowHandle(), 5);
}

void Renderer::DrawFrame()
{
    // Start of Frame
    Executor->BeginFrame();
    Executor->GetSwapChainRenderTargets(CurrentBackBufferView, CurrentBackBuffer);
    ImageMemoryBarrier preBarrier = PRE_BARRIER;
    preBarrier.ImageResource = CurrentBackBuffer;
    Executor->IssueImageMemoryBarrier(preBarrier);
    
    // Call Noesis Pre-Frame Callback
    if (StartOfFrameCallback) StartOfFrameCallback();
    
    // Transition RTVs to read only
    if (!IsInitialized)
    {
        ImageMemoryBarrier initBarrier = INIT_BARRIER;
        for (int i = 0; i < PipelineFrameContexts.size(); i++)
            for (int j = 0; j < PipelineFrameContexts[i].ContextPipeline->GetOwnedImageCount(); j++)
            {
                initBarrier.ImageResource = PipelineFrameContexts[i].ContextPipeline->GetOwnedImage(j);
                Executor->IssueImageMemoryBarrier(initBarrier);
            }
        
        IsInitialized = true;
    }
    
    // This is where the magic happens
    for (uint32_t i = 0; i < PipelineFrameContexts.size(); i++)
        ExecutePipelineContext(i, i == PipelineFrameContexts.size() - 1);
    
    // Noesis End-Frame callback
    Executor->StartNoesisContext(MainWindow->GetWidth(), MainWindow->GetHeight());
    if (EndOfFrameCallback) EndOfFrameCallback();
    Executor->EndNoesisContext();
    
    // End of Frame
    ImageMemoryBarrier postBarrier = POST_BARRIER;
    postBarrier.ImageResource = CurrentBackBuffer;
    Executor->IssueImageMemoryBarrier(postBarrier);
    Executor->EndFrame();
}

int Renderer::End()
{
    for (auto context : PipelineFrameContexts)
    {
        delete context.ContextPipeline;
        context.ContextPipeline = nullptr;
    }
    
    delete BufferAlloc;
    delete Executor;
    delete MainWindow;

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
        Executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
    }
    
    // Begin Pipeline
    if (PipelineFrameContexts[contextIndex].IsPresented)
        Executor->BeginPipeline(PipelineFrameContexts[contextIndex].ContextPipeline, {CurrentBackBufferView}, nullptr, MainWindow->GetWidth(), MainWindow->GetHeight());
    else
        Executor->BeginPipeline(PipelineFrameContexts[contextIndex].ContextPipeline, {}, nullptr, MainWindow->GetWidth(), MainWindow->GetHeight());

    // Bind Descriptors and Draw
    std::vector<uint64_t>* perFrameDescriptors = PipelineFrameContexts[contextIndex].PerFrameDescriptors.size() > 0 
        ? &PipelineFrameContexts[contextIndex].PerFrameDescriptors : nullptr;
    uint32_t numFrameDescs = perFrameDescriptors ? perFrameDescriptors->size() : 0;
    
    Executor->BindPipelineDescriptorSets(perFrameDescriptors);
    
    if (PipelineFrameContexts[contextIndex].IsFSQuad)
        Executor->DrawFSQuad();
    else
        for (int j = 0; j < PipelineFrameContexts[contextIndex].IndexedDraws.size(); j++)
        {
            Executor->BindDrawDescriptorSets(&PipelineFrameContexts[contextIndex].IndexedDraws[j].PerDrawDescriptors, numFrameDescs);
            Executor->DrawIndexed(
                PipelineFrameContexts[contextIndex].IndexedDraws[j].VertexBufferID,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].VertexCount,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].IndexBufferID,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].IndexCount,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].PushConstants,
                PipelineFrameContexts[contextIndex].IndexedDraws[j].PushConstantSize
            );
        }
    
    // End Pipeline
    Executor->EndPipeline();
    
    // Post-draw attachment barrier
    ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
    for (int j = 0; j < PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImageCount(); j++)
    {
        gBufferBarrier.ImageResource = PipelineFrameContexts[contextIndex].ContextPipeline->GetOwnedImage(j);
        Executor->IssueImageMemoryBarrier(gBufferBarrier);
    }
}

void Renderer::SetStartOfFrameCallback(FrameCallback callback)
{
    StartOfFrameCallback = callback;
}

void Renderer::SetRenderCallback(FrameCallback callback)
{
    EndOfFrameCallback = callback;
}

void Renderer::Wait()
{
    Executor->Wait();
}

void Renderer::CreatePipelines(std::vector<RHIStructures::PipelineDesc> pipelineDescs, std::vector<std::vector<DescriptorSetBinding>> vpBindings)
{
    std::vector<class Pipeline*> pipelines;
    for (uint32_t i = 0; i < pipelineDescs.size(); ++i)
    {
        if (i == 0 || pipelines[i-1]->GetOutputResource() == nullptr)
            pipelines.push_back(Pipeline::Create(i, pipelineDescs[i]));
        else
        {    
            std::vector<IOResource> inputResources = {*pipelines[i-1]->GetOutputResource()};
            pipelines.push_back(Pipeline::Create(i, pipelineDescs[i], &inputResources));
        }
        uint64_t set = BufferAllocator::GetInstance()->AllocateDescriptorSet(i, 0, vpBindings[i]);
        Renderer::CreatePipelineFrameContext(pipelines[i], pipelineDescs[i].IsQuad, pipelineDescs[i].IsPresented);
        Renderer::AddDescriptorIDToContext(i, set);
    }
}
