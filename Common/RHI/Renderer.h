#pragma once
#include <vector>

class Window;

struct CoreInitData
{
    bool SwapchainMSAA = false;
    bool SwapchainMSAASamples = 1;
};

class Renderer
{
public:
    static void StartRender(Window* window, CoreInitData data);
    static void EndRender();
    static void BeginFrame();
    static void EndFrame();
    static void GetSwapChainRenderTargets(std::vector<void*>& outColorViews);
};
