#pragma once

#include <array>

class InputState
{
public:
    
    static InputState& GetInstance();
    
    bool IsKeyDown(int keyCode) const;
    bool IsKeyPressed(int keyCode) const;
    bool IsKeyReleased(int keyCode) const;
    
    int GetMouseX() const { return MouseX; }
    int GetMouseY() const { return MouseY; }
    int GetMouseDeltaX() const { return MouseDeltaX; }
    int GetMouseDeltaY() const { return MouseDeltaY; }
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonPressed(int button) const;
    
    void Update();
    
    void SetKeyDown(int keyCode);
    void SetKeyUp(int keyCode);
    void SetMousePosition(int x, int y);
    void SetMouseButtonDown(int button);
    void SetMouseButtonUp(int button);

private:
    
    InputState() = default;

    static constexpr int MAX_KEYS = 256;
    
    std::array<bool, MAX_KEYS> KeyStates = {};
    std::array<bool, MAX_KEYS> KeyPressed = {};
    std::array<bool, MAX_KEYS> KeyReleased = {};
    
    std::array<bool, 5> MouseButtons = {};
    std::array<bool, 5> MousePressed = {};
    std::array<bool, 5> MouseReleased = {};
    
    int MouseX = 0, MouseY = 0;
    int MouseDeltaX = 0, MouseDeltaY = 0;
    int PrevMouseX = 0, PrevMouseY = 0;
};

