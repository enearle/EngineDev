#pragma once
#include <NoesisPCH.h>
class NoesisUILayer
{

    
public:
    void NoesisInit();
    
    void NoesisUpdate(float deltaTime);
    void NoesisRenderOffscreen();
    void NoesisRenderOnscreen();
    void NoesisShutdown();

private:
    double accumulatedTime = 0.0;
    uint64_t frameCounter = 0;
    
    Noesis::Ptr<Noesis::RenderDevice> RenderDevice;
    Noesis::Ptr<Noesis::IView> View;
    
};
