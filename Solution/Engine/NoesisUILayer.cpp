#include "NoesisUILayer.h"

#include <iostream>

#include "ForwardInterface.h"
#include <NsApp/Interaction.h>
#include <NsApp/BackgroundEffectBehavior.h>
#include <NsApp/BehaviorCollection.h>
#include <NsApp/TriggerCollection.h>
#include <NsApp/MouseDragElementBehavior.h>
#include <NsApp/EventTrigger.h>
#include <NsApp/ControlStoryboardAction.h>
#include <NsGui/IntegrationAPI.h>
#include <NsRender/VKFactory.h>
#include <NsRender/D3D12Factory.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsGui/IView.h>
#include <NsGui/IRenderer.h>
#include <NsGui/Button.h>
#include <NsCore/Delegate.h>
#include <NsGui/ResourceDictionary.h>
#include <NsGui/UICollection.h>
#include <print>
#include "Window.h"
#include "Renderer.h"
#include <NsGui/Uri.h>

#include "Input/InputEventSystem.h"

NoesisUILayer& NoesisUILayer::Instance()
{
    static NoesisUILayer instance;
    return instance;
}


void NoesisUILayer::NoesisInit()
{
    Init();
    
    Noesis::GUI::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg)
    {
        const char* prefixes[] = { "T", "D", "I", "W", "E" };
        std::println("[NOESIS/{}] {}", prefixes[level], msg);
    });
    
    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("."));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>("."));
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("."));
    
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
        NoesisApp::VKFactory::WarmUpRenderPass(RenderDevice.GetPtr(), ForwardInterface::GetNoesisCompatibilityRenderPass(), VK_SAMPLE_COUNT_1_BIT);
    }
    else if (ForwardInterface::GetCurrentAPI() == DirectX12)
    {
        {
            ID3D12Fence* fence = nullptr;
            ForwardInterface::GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
            NoesFence = fence;
        }
        frameCounter   = 0;
        accumulatedTime = 0.0;

        RenderDevice = NoesisApp::D3D12Factory::CreateDevice(
            ForwardInterface::GetD3D12Device(),
            static_cast<ID3D12Fence*>(NoesFence),
            ForwardInterface::GetD3D12RenderTargetFormat(),
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            ForwardInterface::GetD3D12SampleDesc(),
            true
            );
    }
    

    Noesis::Ptr<Noesis::ResourceDictionary> theme = Noesis::GUI::LoadXaml<Noesis::ResourceDictionary>(
    Noesis::Uri("../Engine/MyXAML/NoesisTheme.DarkBlue.xaml"));

    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(
        Noesis::Uri("../Engine/MyXAML/MainPage.xaml"));

    xaml->GetResources()->GetMergedDictionaries()->Add(theme.GetPtr());
    
    Noesis::Button* button = xaml->FindName<Noesis::Button>("myButton");
    if (button != nullptr)
    {
        if (button != nullptr)
        {
            button->Click() += Noesis::MakeDelegate(this, &NoesisUILayer::OnButtonClick);
        }
    }
    View = Noesis::GUI::CreateView(xaml);
    View->GetRenderer()->Init(RenderDevice.GetPtr());
    View->SetSize(Renderer::GetWindow()->GetWidth(), Renderer::GetWindow()->GetHeight());
    View->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    
    InputEventSystem::RegisterMouseDeltaCallback([this](float x, float y) { View->MouseMove(x,y); });
    InputEventSystem::RegisterMouseDownCallback([this](float x, float y) { View->MouseButtonDown(x,y, Noesis::MouseButton_Left); });
    InputEventSystem::RegisterMouseUpCallback([this](float x, float y) { View->MouseButtonUp(x,y, Noesis::MouseButton_Left); });
    
    StartOfFrameHandle = Renderer::EventOnStartOfFrame().Subscribe([this]() { this->NoesisRenderOffscreen(); });
    EndOfFrameHandle   = Renderer::EventOnEndOfFrame().Subscribe([this]() { this->NoesisRenderOnscreen(); });
    ResizeHandle       = Renderer::EventOnResize().Subscribe([this](uint32_t width, uint32_t height) { this->OnResize(width, height); });
}

void NoesisUILayer::NoesisUpdate(float deltaTime)
{
    accumulatedTime += deltaTime;
    uint32_t gvX, gvY, gvW, gvH;
    Renderer::GetGameViewport(gvX, gvY, gvW, gvH);
    if (gvW > 0 && gvH > 0)
        View->SetSize(gvW, gvH);
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
    if (ForwardInterface::GetCurrentAPI() == Vulkan)
    {
        NoesisApp::VKFactory::SetRenderPass(RenderDevice.GetPtr(), ForwardInterface::GetNoesisCompatibilityRenderPass(), VK_SAMPLE_COUNT_1_BIT);
        const uint32_t framesInFlight = ForwardInterface::GetVulkanFramesInFlight();
        NoesisApp::VKFactory::RecordingInfo info = {};
        info.commandBuffer = ForwardInterface::GetCommandBuffer();
        info.frameNumber = frameCounter;
        info.safeFrameNumber = frameCounter > framesInFlight ? frameCounter - framesInFlight : 0;
        
        NoesisApp::VKFactory::SetCommandBuffer(RenderDevice.GetPtr(), info);
        View->GetRenderer()->Render(true);
    }
    else if (ForwardInterface::GetCurrentAPI() == DirectX12)
    {
        ID3D12GraphicsCommandList* cmdList = ForwardInterface::GetCommandList();
        uint32_t w = Renderer::GetWindow()->GetWidth();
        uint32_t h = Renderer::GetWindow()->GetHeight();

        uint32_t gvX, gvY, gvW, gvH;
        Renderer::GetGameViewport(gvX, gvY, gvW, gvH);
        D3D12_VIEWPORT vp;
        D3D12_RECT sc;
        if (gvW > 0 && gvH > 0)
        {
            vp = { (FLOAT)gvX, (FLOAT)gvY, (FLOAT)gvW, (FLOAT)gvH, 0.0f, 1.0f };
            sc = { (LONG)gvX, (LONG)gvY, (LONG)(gvX + gvW), (LONG)(gvY + gvH) };
        }
        else
        {
            vp = { 0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f };
            sc = { 0, 0, (LONG)w, (LONG)h };
        }
        cmdList->RSSetViewports(1, &vp);
        cmdList->RSSetScissorRects(1, &sc);

        NoesisApp::D3D12Factory::SetCommandList(RenderDevice.GetPtr(),
            ForwardInterface::GetCommandList(),
            frameCounter);
        View->GetRenderer()->Render();
        NoesisApp::D3D12Factory::EndPendingSplitBarriers(RenderDevice);
    }
}

void NoesisUILayer::NoesisShutdown()
{
    Renderer::EventOnStartOfFrame().Unsubscribe(StartOfFrameHandle);
    Renderer::EventOnEndOfFrame().Unsubscribe(EndOfFrameHandle);
    Renderer::EventOnResize().Unsubscribe(ResizeHandle);
    StartOfFrameHandle = EndOfFrameHandle = ResizeHandle = (size_t)-1;

    View->GetRenderer()->Shutdown();
    View.Reset();
    RenderDevice.Reset();
    if (NoesFence) { static_cast<ID3D12Fence*>(NoesFence)->Release(); NoesFence = nullptr; }
}

void NoesisUILayer::OnResize(uint32_t width, uint32_t height)
{
    if (View)
    {
        uint32_t gvX, gvY, gvW, gvH;
        Renderer::GetGameViewport(gvX, gvY, gvW, gvH);
        View->SetSize(gvW > 0 ? gvW : width, gvH > 0 ? gvH : height);
    }
}

void NoesisUILayer::RegisterComponents() const
{
}

void NoesisUILayer::OnButtonClick(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& e)
{
    std::cout << "Button clicked!" << std::endl;
}