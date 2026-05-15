#include "Renderer.h"
#include <iostream>
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
Event<> Renderer::OnStartOfFrame;
Event<> Renderer::OnEndOfFrame;
Event<uint32_t, uint32_t> Renderer::OnResize;

void Renderer::Start(Window* mainWindow)
{
    MainWindow = mainWindow;
    MainWindow->OnResize.Subscribe([](UINT width, UINT height) { OnResizeEnd(width, height); });
    
    CoreInitData data;
    data.SwapchainMSAA = false;
    data.SwapchainMSAASamples = 1;
    
    Executor = PipelineExecutor::Create(MainWindow, data);
    BufferAlloc = BufferAllocator::GetInstance();
    
    ShowWindow(MainWindow->GetWindowHandle(), 5);
}

void Renderer::DrawFrame()
{
    // Skip draw if currently resizing
    if (MainWindow->IsResizing()) return;
    
    // Start of Frame
    Executor->BeginFrame();
    Executor->GetSwapChainRenderTargets(CurrentBackBufferView, CurrentBackBuffer);
    ImageMemoryBarrier preBarrier = PRE_BARRIER;
    preBarrier.ImageResource = CurrentBackBuffer;
    Executor->IssueImageMemoryBarrier({ preBarrier });
    
    // Call Noesis Pre-Frame Callback
    OnStartOfFrame.Invoke();
    
    // Transition RTVs to read only
    if (!IsInitialized)
    {
        std::vector<ImageMemoryBarrier> initBarriers;
        ImageMemoryBarrier initBarrier = INIT_BARRIER;
        for (int i = 0; i < PipelineFrameContexts.size(); i++)
            for (int j = 0; j < PipelineFrameContexts[i].ContextPipeline->GetOwnedImageCount(); j++)
            {
                initBarrier.ImageResource = PipelineFrameContexts[i].ContextPipeline->GetOwnedImage(j);
                initBarrier.ArrayLayerCount = PipelineFrameContexts[i].ContextPipeline->GetArrayLayerCount();
                initBarriers.push_back(initBarrier);
            }

        ImageMemoryBarrier initDepthBarrier = INIT_DEPTH_BARRIER;
        for (int i = 0; i < PipelineFrameContexts.size(); i++)
            if (PipelineFrameContexts[i].ContextPipeline->GetOwnedDepthImage())
            {
                initDepthBarrier.ImageResource = PipelineFrameContexts[i].ContextPipeline->GetOwnedDepthImage();
                initDepthBarrier.ArrayLayerCount = PipelineFrameContexts[i].ContextPipeline->GetArrayLayerCount();
                initBarriers.push_back(initDepthBarrier);
            }

        Executor->IssueImageMemoryBarrier(initBarriers);
        
        IsInitialized = true;
    }
    
    // This is where the magic happens
    for (uint32_t i = 0; i < PipelineFrameContexts.size(); i++)
        ExecutePipelineContext(i, i == PipelineFrameContexts.size() - 1);
    
    // Noesis End-Frame callback
    Executor->StartNoesisContext(MainWindow->GetWidth(), MainWindow->GetHeight());
    OnEndOfFrame.Invoke();
    Executor->EndNoesisContext();
    
    // End of Frame
    ImageMemoryBarrier postBarrier = POST_BARRIER;
    postBarrier.ImageResource = CurrentBackBuffer;
    Executor->IssueImageMemoryBarrier({ postBarrier });
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
    std::vector<ImageMemoryBarrier> prePassBarriers;
    ImageMemoryBarrier readToAttachmentBarrier = READ_TO_ATTACHMENT_BARRIER;
    for (int j = 0; j < mainPipeline->GetOwnedImageCount(); j++)
    {
        readToAttachmentBarrier.ImageResource = mainPipeline->GetOwnedImage(j);
        readToAttachmentBarrier.ArrayLayerCount = mainPipeline->GetArrayLayerCount();
        prePassBarriers.push_back(readToAttachmentBarrier);
    }

    if (mainPipeline->GetOwnedDepthImage())
    {
        ImageMemoryBarrier depthBarrier = READ_TO_DEPTH_ATTACHMENT_BARRIER;
        depthBarrier.ImageResource = mainPipeline->GetOwnedDepthImage();
        depthBarrier.ArrayLayerCount = mainPipeline->GetArrayLayerCount();
        prePassBarriers.push_back(depthBarrier);
    }

    Executor->IssueImageMemoryBarrier(prePassBarriers);
    
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
    
            bool isFirstInContext = !renderingStarted;
            bool isLastInContext = (i == lastNonEmptyBinIndex);
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
    std::vector<ImageMemoryBarrier> postPassBarriers;
    ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
    for (int j = 0; j < mainPipeline->GetOwnedImageCount(); j++)
    {
        gBufferBarrier.ImageResource = mainPipeline->GetOwnedImage(j);
        gBufferBarrier.ArrayLayerCount = mainPipeline->GetArrayLayerCount();
        postPassBarriers.push_back(gBufferBarrier);
    }

    if (mainPipeline->GetOwnedDepthImage())
    {
        ImageMemoryBarrier depthBarrier = DEPTH_ATTACHMENT_TO_READ_BARRIER;
        depthBarrier.ImageResource = mainPipeline->GetOwnedDepthImage();
        depthBarrier.ArrayLayerCount = mainPipeline->GetArrayLayerCount();
        postPassBarriers.push_back(depthBarrier);
    }

    Executor->IssueImageMemoryBarrier(postPassBarriers);
}

void Renderer::WaitForGpu()
{
    Executor->Wait();
}

void Renderer::ResetPipelines()
{
    WaitForGpu();
    for (auto& context : PipelineFrameContexts)
    {
        delete context.ContextPipeline;
        context.ContextPipeline = nullptr;
    }
    PipelineFrameContexts.clear();
    IsInitialized = false;
}

void Renderer::CreatePipelines(std::vector<RHIStructures::PipelineDesc> pipelineDescs, std::vector<std::vector<PipelineDescriptorData>> pipelineDescriptors)
{
    IsInitialized = false;
    std::vector<class Pipeline*> pipelines;
    
    // Build registry of pipeline outputs by pipeline ID
    std::map<uint32_t, IOResource*> pipelineOutputRegistry;
    
    for (uint32_t i = 0; i < pipelineDescs.size(); ++i)
    {
        std::vector<IOResource*> inputResources;

        // Gather input resource pointers from specified pipelines
        if (!pipelineDescs[i].InputPipelineIDs.empty())
        {
            for (uint32_t inputPipelineID : pipelineDescs[i].InputPipelineIDs)
            {
                auto it = pipelineOutputRegistry.find(inputPipelineID);
                if (it != pipelineOutputRegistry.end() && it->second != nullptr)
                    inputResources.push_back(it->second);
            }
        }

        // Create pipeline with gathered input resources
        Pipeline* pipeline = inputResources.empty()
            ? Pipeline::Create(pipelineDescs[i])
            : Pipeline::Create(pipelineDescs[i], &inputResources);
        
        pipelines.push_back(pipeline);
        
        // Register this pipeline's output for future pipelines
        if (pipeline->GetOutputResource() != nullptr)
        {
            pipelineOutputRegistry[pipelineDescs[i].PipelineID] = pipeline->GetOutputResource();
        }
        
        CreatePipelineFrameContext(pipeline, pipelineDescs[i].IsQuad, pipelineDescs[i].IsPresented);
        for (const auto& descData : pipelineDescriptors[i])
        {
            uint64_t set = BufferAllocator::GetInstance()->AllocateDescriptorSet(
                pipelineDescs[i].PipelineID, descData.setIndex, descData.bindings);
            AddDescriptorIDToContext(i, set);
        }
    }
}

void Renderer::OnResizeEnd(uint32_t width, uint32_t height)
{
    std::cout << "Renderer: Resizing to " << width << "x" << height << std::endl;
    WaitForGpu();

    for (auto& context : PipelineFrameContexts)
    {
        if (!context.ContextPipeline) continue;
        context.ContextPipeline->RecreateAttachments(width, height);
        for (Pipeline* variant : context.ContextPipeline->PipelineVariants)
            variant->RecreateAttachments(width, height);
    }

    // Re-write any descriptor sets that reference the now-recreated attachments
    for (auto& context : PipelineFrameContexts)
        if (context.ContextPipeline)
            context.ContextPipeline->RefreshInputDescriptorSets();

    // New images start in UNDEFINED layout — re-run init barriers next frame
    IsInitialized = false;
    
    Executor->ResetWindow();
    
    // Resize for noesis
    OnResize.Invoke(width, height);
}