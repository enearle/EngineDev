#include "../../Common/RHI/Renderer.h"
#include "../../Common/Window.h"

int main()
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);

    ShowWindow(window->GetWindowHandle(), 5);

    Renderer::StartRender(window);

    while (!window->PeekMessages())
    {
        // Game logic;
        // Renderer;
    }

    Renderer::EndRender();

    delete window;
    return 0;
}
