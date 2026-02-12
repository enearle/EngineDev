#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/RenderPassExecutor.h"
#include <DirectXMath.h>
#include <iostream>

#include "../../Common/GraphicsSettings.h"
#include "../../Common/DirectX12/D3DCore.h"
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
        Pipeline* PBRGeometryPipe = PBRPipeline();
        std::vector<IOResource> inputResources = {*PBRGeometryPipe->GetOutputResource()};
        Pipeline* PBRLightingPipe = DeferredLightingPipeline(&inputResources);
        
        CameraUBO cameraData;
        
        DirectX::XMMATRIX view = DirectX::XMMatrixIdentity() * DirectX::XMMatrixTranslation(0.0f, 0.0f, -5.0f);
        view = DirectX::XMMatrixInverse(nullptr, view);
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f);
        DirectX::XMStoreFloat4x4(&cameraData.ViewProjection, view * projection);
        
        BufferDesc cameraBufferDesc = DefaultConstantBufferDesc;
        cameraBufferDesc.Size = sizeof(CameraUBO);
        cameraBufferDesc.InitialData = &cameraData;
        
        ModelUBO modelData;
        DirectX::XMStoreFloat4x4(&modelData.Model, DirectX::XMMatrixIdentity()); // Set your actual model matrix
        
        BufferDesc modelBufferDesc = DefaultConstantBufferDesc;
        modelBufferDesc.Size = sizeof(ModelUBO);
        modelBufferDesc.InitialData = &modelData;
        
        std::vector<uint64_t> pbrUniformBuffers {};
        
        std::vector<Material> materials;
        materials.push_back(Material("shells_0", Material::PBR));
        materials.push_back(Material("shells_1", Material::PBR));
        
        RootNode meshRoot = GeometryImport::CreateMeshGroup("Shells.fbx", "Shells", DirectX::XMMatrixIdentity());
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}, {0,0,0,1}, {0,0,0,1}};
        bool uploaded = false;
        
        std::vector<uint64_t> materialDescriptorSets;
        
        while (!window->PeekMessages())
        {
            if (GRAPHICS_SETTINGS.APIToUse != Vulkan) 
                throw std::runtime_error("Vulkan is the only supported API for this sample");
            Renderer::BeginFrame();
            
            if (!uploaded)
            {
                // After flush
                pbrUniformBuffers.push_back(bufferAlloc->CreateBuffer(cameraBufferDesc));
                pbrUniformBuffers.push_back(bufferAlloc->CreateBuffer(modelBufferDesc));
                materialDescriptorSets.push_back(materials[0].LoadMaterial(0, 0, pbrUniformBuffers));
                materialDescriptorSets.push_back(materials[1].LoadMaterial(0, 0, pbrUniformBuffers));

                uploaded = true;
            }
            
            Renderer::GetSwapChainRenderTargets(backBufferView, backBuffer);
            
            ImageMemoryBarrier preBarrier = PRE_BARRIER;
            preBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(preBarrier);
            
            executor->Begin(PBRGeometryPipe, {}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);
            
            executor->DrawSceneNode(meshRoot.GetSceneNode(), materialDescriptorSets);
            

            executor->End();
            
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(postBarrier);
            
            Renderer::EndFrame();
        }

        Renderer::Wait();
        delete bufferAlloc;
        delete executor;
        delete PBRGeometryPipe;
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
