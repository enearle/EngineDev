#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"
#include "../../Common/RHI/Pipeline.h"
#include "../../Common/RHI/RHIConstants.h"
#include "../../Common/RHI/RenderPassExecutor.h"
#include <DirectXMath.h>
#include <iostream>
#include "../../Common/DirectX12/D3DCore.h"

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
        Pipeline* NewPipe = CreateRainbowTrianglePipeline();
        RenderPassExecutor* Executor = RenderPassExecutor::Create();
        
        // Pass addresses of the descriptor handles (cast to void*)
        std::vector<void*> colorViews;
        void* depthDescriptor;

        std::vector<DirectX::XMFLOAT4> clearColors {{0,0,0,1}};

        while (!window->PeekMessages())
        {
            Renderer::BeginFrame();
            Renderer::GetSwapChainRenderTargets(colorViews);
            Executor->BindPipeline(NewPipe);
            Executor->Begin(NewPipe, colorViews, nullptr, window->GetWidth(), window->GetHeight(), clearColors, 0);
    
            ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetCommandList().Get();
            cmdList->DrawInstanced(3, 1, 0, 0);
    
            Executor->End();
            Renderer::EndFrame();
        }
        
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
