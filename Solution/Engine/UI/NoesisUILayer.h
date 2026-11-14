#pragma once
#include <NsApp/Launcher.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IView.h>
#include <NsRender/RenderDevice.h>
#include "../ENGINE_API_Macro.h"
#include "Renderer.h"

class ENGINE_API NoesisUILayer : public NoesisApp::Launcher
{
public:
    static NoesisUILayer& Instance();
    void NoesisInit();
    void NoesisUpdate(float deltaTime);
    void NoesisRenderOffscreen();
    void NoesisRenderOnscreen();
    void NoesisShutdown();
    void OnResize(uint32_t width, uint32_t height);
private:
    void OnButtonClick(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& e);

protected:
    void RegisterComponents() const override;

private:
    double accumulatedTime = 0.0;
    uint64_t frameCounter = 0;

    Renderer::ExternalPassID ExternalPassID = (size_t)-1;
    size_t ResizeHandle = (size_t)-1;

    Noesis::Ptr<Noesis::RenderDevice> RenderDevice;
    Noesis::Ptr<Noesis::IView> View;
};
