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
        Pipeline* texturedQuadPipe = TexturedQuadPipeline();
        RenderPassExecutor* executor = RenderPassExecutor::Create();
        BufferAllocator* bufferAlloc = BufferAllocator::Create();
        
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
        uint64_t texture_id;
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}};
        bool uploaded = false;

        while (!window->PeekMessages())
        {
            Renderer::BeginFrame();
            
            if (!uploaded)
            {
                texture_id = bufferAlloc->CreateImage(desc);
                uploaded = true;
            }
            
            Renderer::GetSwapChainRenderTargets(backBufferView, backBuffer);
            
            ImageMemoryBarrier preBarrier = PRE_BARRIER;
            preBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(preBarrier);
            
            //executor->BindPipeline(TrianglePipe);
            //executor->Begin(TrianglePipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);
            executor->BindPipeline(texturedQuadPipe);
            executor->Begin(texturedQuadPipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);
            
            if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
            {
                DirectX12BufferAllocator* alloc = static_cast<DirectX12BufferAllocator*>(bufferAlloc);
                ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetCommandList().Get();
                cmdList->SetDescriptorHeaps(1, alloc->GetShaderResourceHeap().GetAddressOf());
                D3D12_GPU_DESCRIPTOR_HANDLE handle {};
                handle.ptr = alloc->GetImageAllocation(texture_id).Descriptor;
                cmdList->SetGraphicsRootDescriptorTable(0, handle);
                cmdList->DrawInstanced(6, 1, 0, 0);
            }
            else if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
            {
                VulkanBufferAllocator* alloc = static_cast<VulkanBufferAllocator*>(bufferAlloc);
                VulkanPipeline* pipeline = static_cast<VulkanPipeline*>(texturedQuadPipe);
                PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT_FnPtr = VulkanCore::GetInstance().GetVkCmdBindDescriptorBuffersEXT();
                PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT_FnPtr = VulkanCore::GetInstance().GetVkCmdSetDescriptorBufferOffsetsEXT();
                VkCommandBuffer cmdBuffer = VulkanCore::GetInstance().GetCommandBuffer();
                // TODO look descbuffer embedded sampler
                VkDeviceAddress address = alloc->GetDescriptorBufferAddress();
                VkDeviceSize offset = bufferAlloc->GetImageAllocation(texture_id).Descriptor - address;
                uint32_t bufferIndex = 0;
                VkDescriptorBufferBindingInfoEXT info;
                info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
                info.address = address;
                info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                                VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
                info.pNext = nullptr;
                vkCmdBindDescriptorBuffersEXT_FnPtr(cmdBuffer, 1, &info);
                
                auto layout = static_cast<VulkanPipeline*>(texturedQuadPipe)->GetPipelineLayout();
                
                vkCmdSetDescriptorBufferOffsetsEXT_FnPtr(
                    cmdBuffer, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    layout, 
                    0, 
                    1, 
                    &bufferIndex, 
                    &offset);
                vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
            }

            executor->End();
            
            ImageMemoryBarrier postBarrier = POST_BARRIER;
            postBarrier.ImageResource = backBuffer;
            executor->IssueImageMemoryBarrier(postBarrier);
            
            Renderer::EndFrame();
        }

        Renderer::Wait();
        delete bufferAlloc;
        delete executor;        
        //delete TrianglePipe;
        delete texturedQuadPipe;
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
