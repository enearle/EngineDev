#pragma once
#include "ENGINE_API_Macro.h"

class TempGameObject;
class ENGINE_API DerpMover
{
    TempGameObject* Derp;
    bool DanceForward = true;
    float ProgressMove = 0.0f;
    float DurationMove = 3.0f;
    float ProgressDance = 0.0f;
    float DurationDance = 1.0f;
    
public:
    DerpMover(TempGameObject* derp) : Derp(derp) {}
    void MoveTheDerp(double dt);
    
};
