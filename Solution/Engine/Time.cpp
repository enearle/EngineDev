#include "Time.h"
std::chrono::time_point<std::chrono::steady_clock> Time::StartTime;
std::chrono::time_point<std::chrono::steady_clock> Time::LastFrameTime;
std::chrono::time_point<std::chrono::steady_clock> Time::CurrentTime;
double Time::DeltaTime;
double Time::RunningTime;
