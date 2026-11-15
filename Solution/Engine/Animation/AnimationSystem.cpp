#include "AnimationSystem.h"
#include <algorithm>
#include <chrono>
#include <vector>
#include "AnimatorComponent.h"

static std::vector<AnimatorComponent*> Animators;
static bool IsSystemFrozen = false;
static AnimationStats Stats;

void AnimationSystem::Enlist(AnimatorComponent* animator)
{
    Animators.push_back(animator);
}

void AnimationSystem::Retire(AnimatorComponent* animator)
{
    std::erase(Animators, animator);
}

void AnimationSystem::Update(float dt)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    Stats.Characters = 0;
    Stats.Joints = 0;
    for (AnimatorComponent* animator : Animators)
    {
        animator->Update(dt, IsSystemFrozen);
        if (!animator->GetSkeleton())
            continue;
        Stats.Characters++;
        Stats.Joints += animator->GetSkeleton()->GetJointCount();
    }
    Stats.CpuMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void AnimationSystem::SetFrozen(bool isFrozen)
{
    IsSystemFrozen = isFrozen;
}

bool AnimationSystem::IsFrozen()
{
    return IsSystemFrozen;
}

const AnimationStats& AnimationSystem::GetStats()
{
    return Stats;
}
