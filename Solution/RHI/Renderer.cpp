#include "Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/PipelineExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/RHI/Uniform.h"
#include "../../Common/DirectX12/D3DCore.h"
#include "../../Common/Vulkan/VulkanCore.h"
#include "../../Common/RHI/BufferAllocator.h"
#include "../../Common/RHI/Material.h"
#include "../../Common/RHI/Geometry/Mesh.h"
#include "../../Common/RHI/Geometry/GeometryImport.h"

using namespace RHIConstants;

int Renderer::run()
{
    try
    {
        Window* window = new Window(L"MyWindow", Win32, 1280, 720);

        ShowWindow(window->GetWindowHandle(), 5);
        
        CoreInitData data;
        data.SwapchainMSAA = false;
        data.SwapchainMSAASamples = 1;
        
        PipelineExecutor* executor = PipelineExecutor::Create(window, data);
        BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
        Pipeline* PBRGeometryPipe = PBRGeometryPipeline();
        std::vector<IOResource> inputResources = {*PBRGeometryPipe->GetOutputResource()};
        Pipeline* PBRLightingPipe = DeferredLightingPipeline(&inputResources);
        
        DirectX::XMFLOAT4 cameraPosition = {0.0, 10, -8, 1};
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&cameraPosition), DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1));
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f);
        DirectX::XMMATRIX vp = view * projection;
        DirectX::XMStoreFloat4x4(&viewProj, vp);
        
        std::vector<Material> materials;
        materials.push_back(Material("shells_0", Material::PBR));
        materials.push_back(Material("shells_1", Material::PBR));
        
        RootNode meshRoot = GeometryImport::CreateMeshGroup("shells.fbx", "Shells", DirectX::XMMatrixIdentity());
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1}};
        bool initialized = false;
        
        std::vector<uint64_t> materialDescriptorSets;
        Uniform uniform;
        
        while (!window->PeekMessages())
        {
            executor->BeginFrame();
            
            if (!initialized)
            {
                materialDescriptorSets.push_back(materials[0].LoadMaterial(0, 0));
                materialDescriptorSets.push_back(materials[1].LoadMaterial(0, 0));
                
                ImageMemoryBarrier initBarrier = INIT_BARRIER;
                initBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(0);
                executor->IssueImageMemoryBarrier(initBarrier);
                initBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(1);
                executor->IssueImageMemoryBarrier(initBarrier);
                initBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(2);
                executor->IssueImageMemoryBarrier(initBarrier);
                initBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(3);
                executor->IssueImageMemoryBarrier(initBarrier);
                
                ImageMemoryBarrier intDepthBarrier = INIT_DEPTH_BARRIER;
                intDepthBarrier.ImageResource = PBRGeometryPipe->GetOwnedDepthImage();
                executor->IssueImageMemoryBarrier(intDepthBarrier);
                
                initialized = true;
            }
            
            executor->GetSwapChainRenderTargets(backBufferView, backBuffer);
            
            ImageMemoryBarrier preBarrier = PRE_BARRIER;
            preBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(preBarrier);
            
            ImageMemoryBarrier readToAttachmentBarrier = READ_TO_ATTACHMENT_BARRIER;
            readToAttachmentBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(0);
            executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
            readToAttachmentBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(1);
            executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
            readToAttachmentBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(2);
            executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
            readToAttachmentBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(3);
            executor->IssueImageMemoryBarrier(readToAttachmentBarrier);
            
            executor->BeginPipeline(PBRGeometryPipe, {}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 1.0);
            executor->DrawSceneNode(meshRoot.GetSceneNode(), materialDescriptorSets, viewProj, cameraPosition);
            executor->EndPipeline();

            ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(0);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(1);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(2);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(3);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            
            executor->BeginPipeline(PBRLightingPipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), {{0, 0, 0, 1}}, 0);
            executor->DrawQuad(); // Internally references geometry descriptors, no argument needed
            executor->EndPipeline();
            
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(postBarrier);
            
            executor->EndFrame();
        }

        executor->Wait();

        delete PBRGeometryPipe;
        delete PBRLightingPipe;
        
        delete bufferAlloc;
        delete executor;
        delete window;

        return 0;

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
