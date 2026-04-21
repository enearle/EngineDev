#include "Renderer.h"

#include <algorithm>

#include "Window.h"
#include "RHI/Pipeline.h"
#include "RHI/RHIConstants.h"
#include "RHI/PipelineExecutor.h"
#include <DirectXMath.h>
#include "RHI/BufferAllocator.h"

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
        
        ImageMemoryBarrier initDepthBarrier = INIT_DEPTH_BARRIER;
        for (int i = 0; i < PipelineFrameContexts.size(); i++)
            if (PipelineFrameContexts[i].ContextPipeline->GetOwnedDepthImage())
            {
                initDepthBarrier.ImageResource = PipelineFrameContexts[i].ContextPipeline->GetOwnedDepthImage();
                Executor->IssueImageMemoryBarrier(initDepthBarrier);
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
    context.IndexedDrawBins.resize(pipeline->GetPipelineVariantCount() + 1);
    
    PipelineFrameContexts.push_back(context);
    return PipelineFrameContexts.size() - 1;
}

void Renderer::AddIndexedDrawToContext(uint32_t contextIndex, IndexedDraw draw)
{
    if (PipelineFrameContexts[contextIndex].IndexedDrawBins.size() <= draw.PipelineVarientID)
    {
        std::cout << "Pipeline variant ID " << draw.PipelineVarientID << " out of range (max: " 
          << PipelineFrameContexts[contextIndex].IndexedDrawBins.size() - 1 << ")" << std::endl;
        return;
    }
    PipelineFrameContexts[contextIndex].IndexedDrawBins[draw.PipelineVarientID].push_back(draw);
}

void Renderer::AddDescriptorIDToContext(uint32_t contextIndex, uint64_t descriptorID)
{
    PipelineFrameContexts[contextIndex].PerFrameDescriptors.push_back(descriptorID);
}

void Renderer::ExecutePipelineContext(uint32_t contextIndex, bool finalContext)
{
    auto& context = PipelineFrameContexts[contextIndex];
    Pipeline* mainPipeline = context.ContextPipeline;

    // Pre-draw attachment barriers
    ImageMemoryBarrier readToAttachmentBarrier = READ_TO_ATTACHMENT_BARRIER;
    for (int j = 0; j < mainPipeline->GetOwnedImageCount(); j++)
    {
        readToAttachmentBarrier.ImageResource = mainPipeline->GetOwnedImage(j);
        Executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
    }
    
    // Depth barrier if exists
    if (mainPipeline->GetOwnedDepthImage())
    {
        ImageMemoryBarrier depthBarrier = READ_TO_DEPTH_ATTACHMENT_BARRIER;
        depthBarrier.ImageResource = mainPipeline->GetOwnedDepthImage();
        Executor->IssueImageMemoryBarrier(depthBarrier);
    }
    
    // Some of this is in BeginPipeline()
    // But pipeline variants need a reference to main pipelines attachments 
    std::vector<void*> attachmentViews;
    void* depthViewToUse = nullptr;
    if (!context.IsPresented)
    {
        for (int j = 0; j < mainPipeline->GetOwnedImageCount(); j++)
            attachmentViews.push_back(mainPipeline->GetOwnedImageView(j));

        depthViewToUse = mainPipeline->GetOwnedDepthImageView();
    }
    else
    {
        attachmentViews.push_back(CurrentBackBufferView);
        depthViewToUse = nullptr;
    }
    
    
    // Bind per-frame descriptors
    std::vector<uint64_t>* perFrameDescriptors = context.PerFrameDescriptors.size() > 0 
        ? &context.PerFrameDescriptors : nullptr;
    uint32_t numFrameDescs = perFrameDescriptors ? perFrameDescriptors->size() : 0;
    
    // Handle draws
    if (context.IsFSQuad)
    {
        Executor->BeginPipeline(mainPipeline, attachmentViews, depthViewToUse, MainWindow->GetWidth(), MainWindow->GetHeight(), false, true);
        Executor->BindPipelineDescriptorSets(perFrameDescriptors);
        Executor->DrawFSQuad();
        Executor->EndPipeline(true);
    }
    else
    {
        Pipeline* currentPipeline;
        bool renderingStarted = false;
        int lastNonEmptyBinIndex = -1;

        // Find last non-empty bin
        for (int i = context.IndexedDrawBins.size() - 1; i >= 0; i--)
        {
            if (!context.IndexedDrawBins[i].empty())
            {
                lastNonEmptyBinIndex = i;
                break;
            }
        }

        for (int i = 0; i < context.IndexedDrawBins.size(); i++)
        {
            if (context.IndexedDrawBins[i].empty())
                continue;  // Skip empty bins entirely
    
            if (i == 0)
                currentPipeline = mainPipeline;
            else
                currentPipeline = mainPipeline->PipelineVariants[i - 1];
    
            bool isFirstInContext = !renderingStarted;  // ✅ Correct!
            bool isLastInContext = (i == lastNonEmptyBinIndex);  // ✅ Correct!
            bool isVariant = (i > 0);
    
            // Begin pipeline
            if (isVariant)
                Executor->BeginPipeline(currentPipeline, attachmentViews, depthViewToUse, 
                                       MainWindow->GetWidth(), MainWindow->GetHeight(), 
                                       true, isFirstInContext);
            else
                Executor->BeginPipeline(currentPipeline, {}, nullptr, 
                                       MainWindow->GetWidth(), MainWindow->GetHeight(), 
                                       false, isFirstInContext);
    
            renderingStarted = true;
            
            Executor->BindPipelineDescriptorSets(perFrameDescriptors);
            
            for (int j = 0; j < context.IndexedDrawBins[i].size(); j++)
            {
                IndexedDraw& draw = context.IndexedDrawBins[i][j];

                Executor->BindDrawDescriptorSets(&draw.PerDrawDescriptors, numFrameDescs);
                Executor->DrawIndexed(
                    draw.VertexBufferID,
                    draw.VertexCount,
                    draw.IndexBufferID,
                    draw.IndexCount,
                    draw.PushConstants,
                    draw.PushConstantSize,
                    draw.VertexStride
                );
            }
            
            // End Pipeline
            
            Executor->EndPipeline(isLastInContext);
        }
    }
    
    
    // Post-draw attachment barriers
    ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
    for (int j = 0; j < mainPipeline->GetOwnedImageCount(); j++)
    {
        gBufferBarrier.ImageResource = mainPipeline->GetOwnedImage(j);
        Executor->IssueImageMemoryBarrier(gBufferBarrier);
    }
    
    // Depth barrier if exists
    if (mainPipeline->GetOwnedDepthImage())
    {
        ImageMemoryBarrier depthBarrier = DEPTH_ATTACHMENT_TO_READ_BARRIER;
        depthBarrier.ImageResource = mainPipeline->GetOwnedDepthImage();
        Executor->IssueImageMemoryBarrier(depthBarrier);
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
            pipelines.push_back(Pipeline::Create(pipelineDescs[i]));
        else
        {    
            std::vector<IOResource> inputResources = {*pipelines[i-1]->GetOutputResource()};
            pipelines.push_back(Pipeline::Create(pipelineDescs[i], &inputResources));
        }
        uint64_t set = BufferAllocator::GetInstance()->AllocateDescriptorSet(pipelineDescs[i].PipelineID, 0, vpBindings[i]);
        Renderer::CreatePipelineFrameContext(pipelines[i], pipelineDescs[i].IsQuad, pipelineDescs[i].IsPresented);
        Renderer::AddDescriptorIDToContext(i, set);
    }
}
