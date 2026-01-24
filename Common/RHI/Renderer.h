#pragma once

class Window;

class Renderer
{
public:
    static void StartRender(Window* window);
    static void EndRender();
};
