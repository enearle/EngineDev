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
        Pipeline* TrianglePipe = CreateRainbowTrianglePipeline();
        RenderPassExecutor* Executor = RenderPassExecutor::Create();
        
        void* backBufferView;
        void* backBuffer;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}};

        while (!window->PeekMessages())
        {
            Renderer::BeginFrame();
            Renderer::GetSwapChainRenderTargets(backBufferView, backBuffer);
            
            // TODO: declare these barriers as constants
            RHIStructures::ImageMemoryBarrier preBarrier{};
            preBarrier.SrcStage = RHIStructures::PipelineStage::TopOfPipe;
            preBarrier.DstStage = RHIStructures::PipelineStage::ColorAttachmentOutput;
            preBarrier.SrcAccessMask = 0;
            preBarrier.DstAccessMask = static_cast<uint32_t>(RHIStructures::AccessFlag::ColorAttachmentWrite);
            preBarrier.OldLayout = RHIStructures::ImageLayout::Present;
            preBarrier.NewLayout = RHIStructures::ImageLayout::ColorAttachment;
            preBarrier.VkImage = backBuffer;
            Executor->IssueImageMemoryBarrier(preBarrier);
            
            Executor->BindPipeline(TrianglePipe);
            Executor->Begin(TrianglePipe, {backBufferView}, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);

            if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
            {
                ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetCommandList().Get();
                cmdList->DrawInstanced(3, 1, 0, 0);
            }
            else if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
            {
                VkCommandBuffer cmdBuffer = VulkanCore::GetInstance().GetCommandBuffer();
                vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
            }

            Executor->End();
            
            // Transition swapchain image back to PRESENT for presentation
            RHIStructures::ImageMemoryBarrier postBarrier{};
            postBarrier.SrcStage = RHIStructures::PipelineStage::ColorAttachmentOutput;
            postBarrier.DstStage = RHIStructures::PipelineStage::BottomOfPipe;
            postBarrier.SrcAccessMask = static_cast<uint32_t>(RHIStructures::AccessFlag::ColorAttachmentWrite);
            postBarrier.DstAccessMask = 0;
            postBarrier.OldLayout = RHIStructures::ImageLayout::ColorAttachment;
            postBarrier.NewLayout = RHIStructures::ImageLayout::Present;
            postBarrier.VkImage = backBuffer;
            Executor->IssueImageMemoryBarrier(postBarrier);
            
            Renderer::EndFrame();
        }

        Renderer::Wait();
        delete Executor;        
        delete TrianglePipe;
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
