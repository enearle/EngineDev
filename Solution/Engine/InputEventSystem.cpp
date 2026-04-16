#include "InputEventSystem.h"

#include <vector>
#include <windows.h>

std::unordered_map<std::string, KeyState> InputEventSystem::sRegisteredGameplayInput;
std::unordered_map<std::string, KeyState> InputEventSystem::sRegisteredMenuInput;
std::vector<MouseDeltaCallback> InputEventSystem::sMouseCallbacks;
std::vector<MouseClickCallback> InputEventSystem::sMouseClickCallbacks;
CommandQueue InputEventSystem::sCommandQueue;
InputMode InputEventSystem::sInputMode = InputMode::Gameplay;
bool InputEventSystem::sGameplayShowCursor = false;
bool InputEventSystem::sMenuShowCursor = true;
bool InputEventSystem::sPausedShowCursor = true;
bool InputEventSystem::sClampCursorToWindowWhenHidden = true;
bool InputEventSystem::sClampCursorToWindowWhenShown = false;

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

void InputEventSystem::PollInput(double deltaTime)
{
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

