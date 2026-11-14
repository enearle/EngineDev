#pragma once
#include <functional>
#include <string>
#include "../ENGINE_API_Macro.h"

// A small always-on-top ImGui window with frame timing. Systems report through it by enlisting a
// panel, which is drawn as a collapsing section below the built-in statistics.
class ENGINE_API DebugOverlay
{
public:
    using PanelCallback = std::function<void()>;
    using PanelID = size_t;

    static void Start();
    static void Shutdown();
    static void SetVisible(bool isVisible);

    static PanelID EnlistPanel(const std::string& title, PanelCallback callback);
    static void RetirePanel(PanelID panel);
};
