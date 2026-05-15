#include "InputEventSystem.h"
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <Solution/RHI/Windows/WindowsHeaders.h>
#include <string>
std::unordered_map<std::string, KeyState> InputEventSystem::sRegisteredGameplayInput;
std::unordered_map<std::string, KeyState> InputEventSystem::sRegisteredMenuInput;
std::vector<MouseDeltaCallback> InputEventSystem::sMouseCallbacks;
std::vector<MouseClickCallback> InputEventSystem::sMouseUpCallbacks;
std::vector<MouseClickCallback> InputEventSystem::sMouseDownCallbacks;
CommandQueue InputEventSystem::sCommandQueue;
InputMode InputEventSystem::sInputMode = InputMode::Gameplay;
bool InputEventSystem::sGameplayShowCursor = false;
bool InputEventSystem::sMenuShowCursor = true;
bool InputEventSystem::sPausedShowCursor = true;
bool InputEventSystem::sClampCursorToWindowWhenHidden = true;
bool InputEventSystem::sClampCursorToWindowWhenShown = false;

std::string InputEventSystem::ToUniqueKey(const std::string& keys)
{
    std::unordered_set<char> uniqueKeys;
    for (char c : keys)
        uniqueKeys.insert(c);
        
    std::string stringSet = std::string(uniqueKeys.begin(), uniqueKeys.end());
    std::sort(stringSet.begin(), stringSet.end());
    return stringSet;
}

void InputEventSystem::RegisterCommand(InputMode inputMode, const std::string& keyCombination, KeyAction action,
                                       CommandCallback callback)
{
    if (keyCombination.empty() || callback == nullptr)
        return;

    std::string uniqueKey;
    std::unordered_map<std::string, KeyState>::iterator it;
    switch (inputMode)
    {
    case InputMode::Gameplay:
        uniqueKey = ToUniqueKey(keyCombination);
        it = sRegisteredGameplayInput.find(uniqueKey);
        
        if (it != sRegisteredGameplayInput.end())
            it->second.RegisterCommand(action, callback);
        else {
            sRegisteredGameplayInput[uniqueKey] = KeyState();
            sRegisteredGameplayInput[uniqueKey].RegisterCommand(action, callback);
        }
        break;
    case InputMode::UI:
        uniqueKey = ToUniqueKey(keyCombination);
        it = sRegisteredMenuInput.find(uniqueKey);
        
        if (it != sRegisteredMenuInput.end())
            it->second.RegisterCommand(action, callback);
        else {
            sRegisteredMenuInput[uniqueKey] = KeyState();
            sRegisteredMenuInput[uniqueKey].RegisterCommand(action, callback);
        }
        break;
    }
}

void InputEventSystem::PollInput(HWND hwnd, double deltaTime)
{
    static bool wasPressed = false;
    bool isPressed = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(hwnd, &mousePos);
    int x = mousePos.x;
    int y = mousePos.y;
    
    switch (sInputMode)
    {
    case InputMode::Gameplay:
        for (auto& [keyString, keyState] : sRegisteredGameplayInput) 
        {
            bool combinationActive = true;
            for (char key : keyString)
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    combinationActive = false;
                    break;
                }
            
            keyState.Update(combinationActive,sCommandQueue);
        }
        
        for (auto callback : sMouseCallbacks)
            callback(x, y);
        
        if (wasPressed && !isPressed)
        {
            for (auto callback : sMouseUpCallbacks)
                callback(x, y);
        }
        else if (isPressed && !wasPressed)
        {
            for (auto callback : sMouseDownCallbacks)
                callback(x, y);
        }

        wasPressed = isPressed;
        
        break;
    case InputMode::UI:
        for (auto& [keyString, keyState] : sRegisteredMenuInput) 
        {
            bool combinationActive = true;
            for (char key : keyString)
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    combinationActive = false;
                    break;
                }
            
            keyState.Update(combinationActive, sCommandQueue);
        }
        break;
    }
}

