#include "InputState.h"

InputState& InputState::GetInstance()
{
    static InputState instance;
    return instance;
}

bool InputState::IsKeyDown(int keyCode) const
{
    if(keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return KeyStates[keyCode];
}

bool InputState::IsKeyPressed(int keyCode) const
{
    if(keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return KeyPressed[keyCode];
}

bool InputState::IsKeyReleased(int keyCode) const
{
    if(keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return KeyReleased[keyCode];
}

bool InputState::IsMouseButtonDown(int button) const
{
    if(button < 0 || button >= 5) return false;
    return MouseButtons[button];
}

bool InputState::IsMouseButtonPressed(int button) const
{
    if(button < 0 || button >= 5) return false;
    return MousePressed[button];
}

void InputState::SetKeyDown(int keyCode)
{
    if(keyCode < 0 || keyCode >= MAX_KEYS) return;
    KeyStates[keyCode] = true;
    KeyPressed[keyCode] = true;
}

void InputState::SetKeyUp(int keyCode)
{
    if(keyCode < 0 || keyCode >= MAX_KEYS) return;
    KeyStates[keyCode] = false;
    KeyReleased[keyCode] = true;
}

void InputState::SetMousePosition(int x, int y)
{
    PrevMouseX = MouseX;
    PrevMouseY = MouseY;
    MouseX = x;
    MouseY = y;
    MouseDeltaX = MouseX - PrevMouseX;
    MouseDeltaY = MouseY - PrevMouseY;
}

void InputState::SetMouseButtonDown(int button)
{
    if(button < 0 || button >= 5) return;
    MouseButtons[button] = true;
    MousePressed[button] = true;
}

void InputState::SetMouseButtonUp(int button)
{
    if(button < 0 || button >= 5) return;
    MouseButtons[button] = false;
    MouseReleased[button] = true;
}

void InputState::Update()
{
    for(int i = 0; i < MAX_KEYS; ++i)
    {
        KeyPressed[i] = false;
        KeyReleased[i] = false;
    }

    for(int i = 0; i < 5; ++i)
    {
        MousePressed[i] = false;
        MouseReleased[i] = false;
    }
}