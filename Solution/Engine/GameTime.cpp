#include "GameTime.h"
#include <chrono>

static std::chrono::time_point<std::chrono::steady_clock> StartTime, LastFrameTime, CurrentTime;
double GameTime::DeltaTime;
double GameTime::RunningTime;

void GameTime::Init()
{
    StartTime = std::chrono::high_resolution_clock::now();
    LastFrameTime = StartTime;
    CurrentTime = StartTime;
}

void GameTime::UpdateTime()
{
    LastFrameTime = CurrentTime;
    CurrentTime = std::chrono::high_resolution_clock::now();
    RunningTime = std::chrono::duration_cast<std::chrono::duration<double>>(CurrentTime - StartTime).count();
    DeltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(CurrentTime - LastFrameTime).count();
}
