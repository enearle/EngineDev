#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/RenderPassExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/DirectX12/D3DCore.h"
#include "../../Common/Vulkan/VulkanCore.h"
#include "../../Common/GraphicsSettings.h"
#include "../../Common/RHI/BufferAllocator.h"
#include "../../Common/RHI/Image/ImageImport.h"

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
        //Pipeline* TrianglePipe = CreateRainbowTrianglePipeline();
        Pipeline* TexturedQuadPipe = TexturedQuadPipeline();
        RenderPassExecutor* Executor = RenderPassExecutor::Create();
        BufferAllocator* BufferAlloc = BufferAllocator::Create();
        
        ImageImport* texture = new ImageImport("Textures/texture");
        
        ImageUsage usage = 
        {
            .TransferSource = false,
            .TransferDestination = true,
            .Type = ImageType::Sampled
        };
        
        MemoryAccess access;
        access.SetGPURead(true);
        
        ImageDesc desc = 
            {
            .Width = texture->GetTextureData().Width,
            .Height = texture->GetTextureData().Height,
            .Size = texture->GetTextureData().TotalSize,
            .Format = Format::R8G8B8A8_UNORM,
            .Usage = usage,
            .Type = ImageType::Sampled,
            .Access = access,
            .Layout = ImageLayout::General,
            .InitialData = texture->GetTextureData().Pixels
        };
        uint64_t texture_id = BufferAlloc->CreateImage(desc);
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}};

        while (!window->PeekMessages())
        {
            Renderer::BeginFrame();
            Renderer::GetSwapChainRenderTargets(backBufferView, backBuffer);
            
            ImageMemoryBarrier preBarrier = PRE_BARRIER;
            preBarrier.ImageResource = backBuffer;
            Executor->IssueImageMemoryBarrier(preBarrier);
            
            //Executor->BindPipeline(TrianglePipe);
            //Executor->Begin(TrianglePipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);
            Executor->BindPipeline(TexturedQuadPipe);
            Executor->Begin(TexturedQuadPipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);

            if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
            {
                ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetCommandList().Get();
                cmdList->DrawInstanced(6, 1, 0, 0);
            }
            else if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
            {
                PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT_FnPtr = VulkanCore::GetInstance().GetVkCmdBindDescriptorBuffersEXT();
                PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT_FnPtr = VulkanCore::GetInstance().GetVkCmdSetDescriptorBufferOffsetsEXT();
                VkCommandBuffer cmdBuffer = VulkanCore::GetInstance().GetCommandBuffer();
                // TODO look descbuffer embedded sampler
                VkDeviceAddress address = static_cast<VulkanBufferAllocator*>(BufferAlloc)->GetDescriptorBufferAddress();
                VkDeviceSize offset = BufferAlloc->GetImageAllocation(texture_id).Descriptor - address;
                uint32_t bufferIndex = 0;
                VkDescriptorBufferBindingInfoEXT info;
                info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
                info.address = address;
                info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                                VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
                info.pNext = nullptr;
                vkCmdBindDescriptorBuffersEXT_FnPtr(cmdBuffer, 1, &info);
                vkCmdSetDescriptorBufferOffsetsEXT_FnPtr(
                    cmdBuffer, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    static_cast<VulkanPipeline*>(TexturedQuadPipe)->GetPipelineLayout(), 
                    0, 
                    1, 
                    &bufferIndex, 
                    &offset);
                vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
            }

            Executor->End();
            
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            Executor->IssueImageMemoryBarrier(postBarrier);
            
            Renderer::EndFrame();
        }

        Renderer::Wait();
        delete BufferAlloc;
        delete Executor;        
        //delete TrianglePipe;
        delete TexturedQuadPipe;
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
