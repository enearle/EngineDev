#pragma once
#include <cstdint>
#include "../ENGINE_API_Macro.h"

class AnimatorComponent;

struct AnimationStats
{
    uint32_t Characters = 0;
    uint32_t Joints = 0;
    double CpuMilliseconds = 0.0;
};

// Evaluates every enlisted animator once per frame. One per program.
class ENGINE_API AnimationSystem
{
public:
    static void Enlist(AnimatorComponent* animator);
    static void Retire(AnimatorComponent* animator);

    static void Update(float dt);

    // A frozen system holds every animator at time 0.
    static void SetFrozen(bool isFrozen);
    static bool IsFrozen();

    static const AnimationStats& GetStats();
};
