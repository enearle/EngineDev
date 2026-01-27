#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"

int main()
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);

    ShowWindow(window->GetWindowHandle(), 5);
    
    CoreInitData data;
    data.SwapchainMSAA = false;
    data.SwapchainMSAASamples = 1;
    
    Renderer::StartRender(window, data);

    

    while (!window->PeekMessages())
    {
        // Game logic;
        // Renderer;
    }

    Renderer::EndRender();

    delete window;
    return 0;
}
