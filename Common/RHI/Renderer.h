#pragma once

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
};
