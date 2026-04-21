#pragma once
#include <windows.h>
#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <string>


namespace DirectX
{
    struct XMFLOAT2;
}

enum KeyAction
{
    Pressed,
    Released,
    Held
};

enum class InputMode { Gameplay, UI };

using CommandCallback = std::function<void (double)>;
using MouseDeltaCallback = std::function<void (float, float)>;
using MouseClickCallback = std::function<void (float, float)>;

class CommandQueue {
    std::queue<CommandCallback> mQueue;
    
public:
    void push(const CommandCallback& cmd) {
        mQueue.push(cmd);
    }
    
    CommandCallback pop() {
        CommandCallback cmd = mQueue.front();
        mQueue.pop();
        return cmd;
    }
    
    bool isEmpty() const {
        return mQueue.empty();
    }
    
    void clear() {
        while (!mQueue.empty())
            mQueue.pop();
    }
};

class KeyState {
    bool isDown = false;
    bool wasDown = false;
    
    std::vector<CommandCallback> registeredPressedCmds;
    std::vector<CommandCallback> registeredReleasedCmds;
    std::vector<CommandCallback> registeredHeldCmds;
    
public:
    KeyState() = default;
    
    void RegisterCommand(KeyAction action, CommandCallback registeredCommandType) 
    {
        switch (action) {
        case Pressed:
            registeredPressedCmds.push_back(registeredCommandType);
            break;
        case Released:
            registeredReleasedCmds.push_back(registeredCommandType);
            break;
        case Held:
            registeredHeldCmds.push_back(registeredCommandType);
            break;
        }
    }
    
    void Update(bool keyValue, CommandQueue& queue) 
    {
        wasDown = isDown;
        isDown = keyValue;
        
        if (wasDown && !isDown)
            for (const auto& cmd : registeredReleasedCmds)
                queue.push(cmd);
        
        if (!wasDown && isDown)
            for (const auto& cmd : registeredPressedCmds)
                queue.push(cmd);
        
        if (isDown)
            for (const auto& cmd : registeredHeldCmds)
                queue.push(cmd);
    }
};

class InputEventSystem 
{
    static std::unordered_map<std::string, KeyState> sRegisteredGameplayInput;
    static std::unordered_map<std::string, KeyState> sRegisteredMenuInput;
    static std::vector<MouseDeltaCallback> sMouseCallbacks;
    static std::vector<MouseClickCallback> sMouseUpCallbacks;
    static std::vector<MouseClickCallback> sMouseDownCallbacks;
    static CommandQueue sCommandQueue;
    static InputMode sInputMode;
    static InputMode sNextInputMode;
    static bool sPausedShowCursor;
    static bool sMenuShowCursor;
    static bool sGameplayShowCursor;
    static bool sClampCursorToWindowWhenHidden;
    static bool sClampCursorToWindowWhenShown;
    
    static std::string ToUniqueKey(const std::string& keys) 
    {
        std::unordered_set<char> uniqueKeys;
        for (char c : keys)
            uniqueKeys.insert(c);
        
        std::string stringSet = std::string(uniqueKeys.begin(), uniqueKeys.end());
        std::sort(stringSet.begin(), stringSet.end());
        return stringSet;
    }
    
public:

    static void RegisterCommand(InputMode inputMode, const std::string& keyCombination, KeyAction action, CommandCallback callback);
    static void PollInput(HWND hwnd, double deltaTime);
    static void ChangeInputMode(InputMode inputMode) { sInputMode = inputMode; }
    static void ProcessCommands(double deltaTime) { while (!sCommandQueue.isEmpty()) sCommandQueue.pop()(deltaTime); }
    static void ClearCommands() { sCommandQueue.clear(); }
    
    static void SetCursorActiveStates(bool gameplayActive = false, bool menuActive = true, bool pausedActive = true)
    {
        sGameplayShowCursor = gameplayActive;
        sMenuShowCursor = menuActive;
        sPausedShowCursor = pausedActive;
    }
    
    static void SetCursorClampWhenHidden(bool clampCursorActive) { sClampCursorToWindowWhenHidden = clampCursorActive; }
    static void SetCursorClampWhenShown(bool clampCursorActive) { sClampCursorToWindowWhenShown = clampCursorActive; }
    static void RegisterMouseDeltaCallback(MouseDeltaCallback callback) { sMouseCallbacks.push_back(callback); }
    static void RegisterMouseUpCallback(MouseClickCallback callback) { sMouseUpCallbacks.push_back(callback); }
    static void RegisterMouseDownCallback(MouseClickCallback callback) { sMouseDownCallbacks.push_back(callback); }
};

