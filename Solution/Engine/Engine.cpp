#include "../RHI/Renderer.h"
#include "Common/Window.h"

int main(int argc, char* argv[])
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);
    
    Renderer::Start(window);
    
    Renderer::Run();
    
    return Renderer::End();
}
