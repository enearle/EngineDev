#pragma once
#include <NsApp/Launcher.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IView.h>
#include <NsRender/RenderDevice.h>

class NoesisUILayer : public NoesisApp::Launcher
{
public:
    void NoesisInit();
    void NoesisUpdate(float deltaTime);
    void NoesisRenderOffscreen();
    void NoesisRenderOnscreen();
    void NoesisShutdown();

protected:
    // Override from Launcher to register custom components (if any)
    void RegisterComponents() const override;

private:
    double accumulatedTime = 0.0;
    uint64_t frameCounter = 0;
    
    Noesis::Ptr<Noesis::RenderDevice> RenderDevice;
    Noesis::Ptr<Noesis::IView> View;
};
