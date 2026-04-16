#pragma once
#include <chrono>

class Time
{
    static std::chrono::time_point<std::chrono::steady_clock> StartTime;
    static std::chrono::time_point<std::chrono::steady_clock> LastFrameTime;
    static std::chrono::time_point<std::chrono::steady_clock> CurrentTime;
    static double DeltaTime;
    static double RunningTime;
    
public:
    
    static void Init()
    {
        StartTime = std::chrono::high_resolution_clock::now();
        LastFrameTime = StartTime;
        CurrentTime = StartTime;
    }
    
    static void UpdateTime()
    {
        LastFrameTime = CurrentTime;
        CurrentTime = std::chrono::high_resolution_clock::now();
        RunningTime = std::chrono::duration_cast<std::chrono::duration<double>>(CurrentTime - StartTime).count();
        DeltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(CurrentTime - LastFrameTime).count();
    }
       
    static double GetRunningTime() { return RunningTime; }
    static double GetDeltaTime() { return DeltaTime; }
};
