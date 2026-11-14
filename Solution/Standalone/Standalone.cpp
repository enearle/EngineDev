#include "Renderer.h"
#include "Window.h"
#include "GraphicsSettings.h"
#include "UI/ImGuiLayer.h"
#include "UI/DebugOverlay.h"
#include "../Game/Game.h"

int main(int argumentCount, char** arguments)
{
    GRAPHICS_SETTINGS.ApplyCommandLine(argumentCount, arguments);

    Window* window = new Window(L"Standalone", Win32, 1280, 720);
    Renderer::Start(window);
    ShowWindow(window->GetWindowHandle(), SW_SHOW);

    ImGuiLayer::Start(false);
    DebugOverlay::Start();

    GameInit();

    while (!window->PeekMessages())
    {
        GameRunFrame(0.0f);
        Renderer::DrawFrame();
    }

    Renderer::WaitForGpu();
    GameShutdown();
    DebugOverlay::Shutdown();
    ImGuiLayer::Shutdown();
    return Renderer::End();
}
