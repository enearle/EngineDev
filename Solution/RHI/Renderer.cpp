#include "Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/PipelineExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/RHI/Uniform.h"
#include "../../Common/RHI/BufferAllocator.h"
#include "../../Common/RHI/Material.h"
#include "../../Common/RHI/Geometry/Mesh.h"
#include "../../Common/RHI/Geometry/GeometryImport.h"

using namespace RHIConstants;

PipelineExecutor* Renderer::executor;
BufferAllocator* Renderer::bufferAlloc;
Window* Renderer::window;
bool Renderer::initialized = false;
std::vector<PipelineFrameContext> Renderer::PipelineFrameContexts;

// temp
Pipeline* Renderer::PBRGeometryPipe;
Pipeline* Renderer::PBRLightingPipe;

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

int Renderer::Run()
{
    try
    {
        PBRGeometryPipe = PBRGeometryPipeline();
        std::vector<IOResource> inputResources = {*PBRGeometryPipe->GetOutputResource()};
        PBRLightingPipe = DeferredLightingPipeline(&inputResources);
        
        DirectX::XMFLOAT4 cameraPosition = {0.0, 10, -8, 1};
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&cameraPosition), DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1));
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f);
        DirectX::XMMATRIX vp = view * projection;
        DirectX::XMStoreFloat4x4(&viewProj, vp);
        
        std::vector<uint64_t> materials;
        materials.push_back(Material("shells_0", Material::PBR).LoadMaterial(0,0));
        materials.push_back(Material("shells_1", Material::PBR).LoadMaterial(0,0));

        RootNode meshRoot = GeometryImport::CreateMeshGroup("shells.fbx", "Shells", DirectX::XMMatrixIdentity());
        

        Uniform uniform;
        
        while (!window->PeekMessages())
        {
            // Start of Frame
            void* backBufferView;
            void* backBuffer;
            executor->BeginFrame();
            executor->GetSwapChainRenderTargets(backBufferView, backBuffer);
            ImageMemoryBarrier preBarrier = PRE_BARRIER;
            preBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(preBarrier);
            
            // Load/unload time
            if (!initialized)
            {
                
                ImageMemoryBarrier initBarrier = INIT_BARRIER;
                for (int i = 0; i < PBRGeometryPipe->GetOwnedImageCount(); i++)
                {
                    initBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(i);
                    executor->IssueImageMemoryBarrier(initBarrier);
                }
                
                initialized = true;
            }
            
            // Pipeline/Draw time
            ImageMemoryBarrier readToAttachmentBarrier = READ_TO_ATTACHMENT_BARRIER;
            for (int i = 0; i < PBRGeometryPipe->GetOwnedImageCount(); i++)
            {
                readToAttachmentBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(i);
                executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
            }
            
            executor->BeginPipeline(PBRGeometryPipe, {}, nullptr, window->GetWidth(), window->GetHeight());
            executor->DrawSceneNode(meshRoot.GetSceneNode(), materials, viewProj, cameraPosition);
            executor->EndPipeline();
            
            ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
            for (int i = 0; i < PBRGeometryPipe->GetOwnedImageCount(); i++)
            {
                
                gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(i);
                executor->IssueImageMemoryBarrier(gBufferBarrier);
            }
            
            executor->BeginPipeline(PBRLightingPipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight());
            executor->DrawQuad(); 
            executor->EndPipeline();
            
            
            // End of Frame
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(postBarrier);
            executor->EndFrame();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }

}

int Renderer::End()
{
    executor->Wait();

    delete PBRGeometryPipe;
    delete PBRLightingPipe;
    delete bufferAlloc;
    delete executor;
    delete window;

    return 0;
}

uint32_t Renderer::CreatePipelineFrameContext(Pipeline* pipeline, bool isQuad)
{
    PipelineFrameContext context;
    context.ContextPipeline = pipeline;
    context.IsQuad = isQuad;
    PipelineFrameContexts.push_back(context);
    return PipelineFrameContexts.size() - 1;
}

void Renderer::AddIndexedDrawToContext(uint32_t contextIndex, RHIStructures::IndexedDraw draw)
{
    PipelineFrameContexts[contextIndex].IndexedDraws.push_back(draw);
}
