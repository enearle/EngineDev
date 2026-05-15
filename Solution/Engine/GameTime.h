#pragma once


class GameTime
{
    static double DeltaTime;
    static double RunningTime;

public:

    static void Init();
    static void UpdateTime();
    static double GetRunningTime() { return RunningTime; }
    static double GetDeltaTime() { return DeltaTime; }
};
