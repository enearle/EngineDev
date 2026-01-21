#include "../../Common/Window.h"
#include "../../Common/DirectX12/D3DCore.h"

int main()
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);

    ShowWindow(window->GetWindowHandle(), 5);

    D3DCore::GetInstance().InitDirect3D(window);

    while (!window->PeekMessages())
    {
        // Game logic;
        // Renderer;
    }

    delete window;
    return 0;
}
