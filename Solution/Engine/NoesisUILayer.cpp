#include "NoesisUILayer.h"
#include "Common/ForwardInterface.h"
#include <NsGui/IntegrationAPI.h>
#include <NsRender/VKFactory.h>
#include <NsRender/D3D12Factory.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsGui/IView.h>
#include <NsGui/IRenderer.h>
#include <print>
#include "Common/Window.h"
#include "Solution/RHI/Renderer.h"

void NoesisUILayer::NoesisInit()
{
    Noesis::GUI::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg)
    {
        const char* prefixes[] = { "T", "D", "I", "W", "E" };
        std::println("[NOESIS/{}] {}", prefixes[level], msg);
    });
    Noesis::GUI::Init();
    
    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("./Assets"));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>("./Assets"));
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("./Assets"));
    
    if (ForwardInterface::GetCurrentAPI() == Vulkan)
    {
        NoesisApp::VKFactory::InstanceInfo info = {};
        info.instance = ForwardInterface::GetVkInstance();
        info.physicalDevice = ForwardInterface::GetVkPhysicalDevice();
        info.device = ForwardInterface::GetVkDevice();
        info.queueFamilyIndex = ForwardInterface::GetVkQueueFamilyIndex();
        info.vkGetInstanceProcAddr = ForwardInterface::GetInstanceProcAddress();
        info.pipelineCache = VK_NULL_HANDLE;
        info.stereoSupport = false;
        
        RenderDevice = NoesisApp::VKFactory::CreateDevice(true, info);
    }
    else if (ForwardInterface::GetCurrentAPI() == DirectX12)
    {
        RenderDevice = NoesisApp::D3D12Factory::CreateDevice(
            ForwardInterface::GetD3D12Device(), 
            ForwardInterface::GetD3D12Fence(),
            ForwardInterface::GetD3D12RenderTargetFormat(),
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            ForwardInterface::GetD3D12SampleDesc(),
            true
            );
    }
    
    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>("YourUI.xaml");
    View = Noesis::GUI::CreateView(xaml);
    View->GetRenderer()->Init(RenderDevice.GetPtr());
    View->SetSize(Renderer::GetWindow()->GetWidth(), Renderer::GetWindow()->GetHeight());
    View->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
}

void NoesisUILayer::NoesisUpdate(float deltaTime)
{
    accumulatedTime += deltaTime;
    View->Update(accumulatedTime);
}

void NoesisUILayer::NoesisRenderOffscreen()
{
    frameCounter++;
    
    if (ForwardInterface::GetCurrentAPI() == Vulkan)
    {
        NoesisApp::VKFactory::RecordingInfo info = {};
        info.commandBuffer = ForwardInterface::GetCommandBuffer();
        info.frameNumber = frameCounter;
        info.safeFrameNumber = frameCounter > 2 ? frameCounter - 2 : 0;
        
        NoesisApp::VKFactory::SetCommandBuffer(RenderDevice.GetPtr(), info);
        NoesisApp::VKFactory::SetRenderPass(RenderDevice.GetPtr(), ForwardInterface::GetNoesisCompatibilityRenderPass(), VK_SAMPLE_COUNT_1_BIT);
    }
    else if (ForwardInterface::GetCurrentAPI() == DirectX12)
    {
        NoesisApp::D3D12Factory::SetCommandList(RenderDevice.GetPtr(), 
            ForwardInterface::GetCommandList(),
            frameCounter);
    }
    
    View->GetRenderer()->UpdateRenderTree();
    View->GetRenderer()->RenderOffscreen();
}

void NoesisUILayer::NoesisRenderOnscreen()
{
    View->GetRenderer()->Render();
}

void NoesisUILayer::NoesisShutdown()
{
    View->GetRenderer()->Shutdown();
    View.Reset();
    RenderDevice.Reset();
}