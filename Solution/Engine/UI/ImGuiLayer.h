#pragma once
#include <functional>
#include "../ENGINE_API_Macro.h"

// Owns the Dear ImGui context and its platform and renderer backends, and attaches it to the
// renderer as an external pass. Anything that wants to draw ImGui widgets subscribes a callback.
class ENGINE_API ImGuiLayer
{
public:
    using DrawCallback = std::function<void()>;
    using Subscription = size_t;

    static void Start(bool isLayoutSavedToDisk);
    static void Shutdown();
    static bool IsStarted();

    static Subscription Subscribe(DrawCallback callback);
    static void Unsubscribe(Subscription subscription);
};
