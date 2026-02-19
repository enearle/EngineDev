#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/RenderPassExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/RHI/Uniform.h"
#include "../../Common/GraphicsSettings.h"
#include "../../Common/DirectX12/D3DCore.h"
#include "../../Common/Vulkan/VulkanCore.h"
#include "../../Common/RHI/BufferAllocator.h"
#include "../../Common/RHI/Material.h"
#include "../../Common/RHI/Geometry/Mesh.h"
#include "../../Common/RHI/Geometry/GeometryImport.h"

using namespace RHIConstants;

int main()
{
    try
    {
        Window* window = new Window(L"MyWindow", Win32, 1280, 720);

        ShowWindow(window->GetWindowHandle(), 5);
        
        CoreInitData data;
        data.SwapchainMSAA = false;
        data.SwapchainMSAASamples = 1;
        
        Renderer::StartRender(window, data);
        RenderPassExecutor* executor = RenderPassExecutor::Create();
        BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
        Pipeline* PBRGeometryPipe = PBRGeometryPipeline();
        std::vector<IOResource> inputResources = {*PBRGeometryPipe->GetOutputResource()};
        Pipeline* PBRLightingPipe = DeferredLightingPipeline(&inputResources);
        
        CameraUBO cameraData;
        
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(0.0, 10, -8, 1), DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1));
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f);
        DirectX::XMMATRIX vp = view * projection;
        DirectX::XMStoreFloat4x4(&cameraData.ViewProjection, vp);
        
        std::vector<uint64_t> pbrUniformBuffers {};
        
        std::vector<Material> materials;
        materials.push_back(Material("shells_0", Material::PBR));
        materials.push_back(Material("shells_1", Material::PBR));
        
        RootNode meshRoot = GeometryImport::CreateMeshGroup("shells.fbx", "Shells", DirectX::XMMatrixIdentity());
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1}};
        bool initialized = false;
        
        //std::vector<std::vector<uint64_t>> drawSets = {};
        std::vector<uint64_t> materialDescriptorSets;
        Uniform uniform;
        
        while (!window->PeekMessages())
        {
            if (GRAPHICS_SETTINGS.APIToUse != Vulkan) 
                throw std::runtime_error("Vulkan is the only supported API for this sample");
            Renderer::BeginFrame();
            
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
            
            Renderer::GetSwapChainRenderTargets(backBufferView, backBuffer);
            
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
            
            executor->Begin(PBRGeometryPipe, {}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 1.0);
            executor->DrawSceneNode(meshRoot.GetSceneNode(), materialDescriptorSets, cameraData.ViewProjection);
            executor->End();
            
            ImageMemoryBarrier gBufferBarrier = ATTACHMENT_TO_READ_BARRIER;
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(0);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(1);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(2);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            gBufferBarrier.ImageResource = PBRGeometryPipe->GetOwnedImage(3);
            executor->IssueImageMemoryBarrier(gBufferBarrier);
            
            executor->Begin(PBRLightingPipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), {{0, 0, 0, 1}}, 0);
            executor->DrawQuad(); // Internally references geometry descriptor, no argument needed
            executor->End();
            
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(postBarrier);
            
            Renderer::EndFrame();
        }

        Renderer::Wait();
        // Delete pipelines FIRST (before buffer allocator)
        delete PBRGeometryPipe;
        delete PBRLightingPipe;  // Don't forget this one!
        delete executor;

        // Delete buffer allocator LAST
        delete bufferAlloc;
        //delete TrianglePipe;
        //delete texturedQuadPipe;
        Renderer::EndRender();
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
