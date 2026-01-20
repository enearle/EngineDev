#include "../../Common/Window.h"

int main()
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);

    ShowWindow(window->GetWindowHandle(), 5);

    while (!window->PeekMessages())
    {
        // Game logic;
        // Renderer;
    }

    delete window;
    return 0;
}
