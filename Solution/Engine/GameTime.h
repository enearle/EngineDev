#pragma once
#include "ENGINE_API_Macro.h"

class ENGINE_API GameTime
{
    static double DeltaTime;
    static double RunningTime;

public:

    static void Init();
    static void UpdateTime();
    static double GetRunningTime() { return RunningTime; }
    static double GetDeltaTime() { return DeltaTime; }
};
