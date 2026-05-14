#include "../RHI/Renderer.h"
#include "Window.h"
#include "../Game/Game.h"

int main()
{
    Window* window = new Window(L"Standalone", Win32, 1280, 720);
    Renderer::Start(window);
    ShowWindow(window->GetWindowHandle(), SW_SHOW);

    GameInit();

    while (!window->PeekMessages())
    {
        GameRunFrame(0.0f);
        Renderer::DrawFrame();
    }

    Renderer::WaitForGpu();
    GameShutdown();
    return Renderer::End();
}
