#include "AnimatorComponent.h"
#include <cmath>
#include "AnimationSystem.h"

using namespace DirectX;

AnimatorComponent::AnimatorComponent(SceneNode* owner) : SceneComponentBase({ Field(ResourceType::Skeleton, "Skeleton") }, owner)
{
    AnimationSystem::Enlist(this);
}

AnimatorComponent::~AnimatorComponent()
{
    AnimationSystem::Retire(this);
}

void AnimatorComponent::Deserialize(std::string& data, long& offset)
{
    SceneComponentBase::Deserialize(data, offset);
    if (Fields[0].GetID().IsValid())
        SetSkeleton(static_cast<Skeleton*>(ResourceManager::GetAsset(Fields[0].GetID(), ResourceType::Skeleton)));
}

void AnimatorComponent::SetSkeleton(const Skeleton* skeleton)
{
    ModelSkeleton = skeleton;
    if (!skeleton)
        return;
    Fields[0].SetID(skeleton->GetID());
    CurrentPose.SetToBind(*skeleton);
    Pose::BuildPalette(*skeleton, CurrentPose, Palette, &JointPositions);
    UpdateBounds();
}

void AnimatorComponent::AddState(const AnimationState& state)
{
    States.push_back(state);
}

bool AnimatorComponent::Play(const std::string& state, float fadeSeconds)
{
    for (size_t i = 0; i < States.size(); i++)
    {
        if (States[i].Name != state)
            continue;

        if (CurrentState >= 0 && fadeSeconds > 0.0f)
        {
            PreviousState = CurrentState;
            PreviousTime = CurrentTime;
            FadeSeconds = fadeSeconds;
            FadeElapsed = 0.0f;
        }
        else
            PreviousState = -1;
        CurrentState = static_cast<int32_t>(i);
        CurrentTime = 0.0f;
        return true;
    }
    return false;
}

void AnimatorComponent::Advance(int32_t state, float& time, float dt, bool isCurrent)
{
    const AnimationState& animation = States[state];
    float duration = animation.StateClip->GetDuration();
    time += dt * animation.Speed;
    if (time < duration || duration <= 0.0f)
        return;

    if (animation.IsLooping)
        time = std::fmod(time, duration);
    else if (isCurrent && !animation.Next.empty())
    {
        float overshoot = time - duration;
        Play(animation.Next, animation.NextFadeSeconds);
        CurrentTime = overshoot;
    }
    else
        time = duration;
}

void AnimatorComponent::Update(float dt, bool isFrozen)
{
    if (!ModelSkeleton)
        return;

    if (CurrentState < 0)
        CurrentPose.SetToBind(*ModelSkeleton);
    else if (isFrozen)
        States[CurrentState].StateClip->Sample(*ModelSkeleton, 0.0f, CurrentPose);
    else
    {
        if (PreviousState >= 0)
        {
            Advance(PreviousState, PreviousTime, dt, false);
            FadeElapsed += dt;
            if (FadeElapsed >= FadeSeconds)
                PreviousState = -1;
        }
        Advance(CurrentState, CurrentTime, dt, true);
        States[CurrentState].StateClip->Sample(*ModelSkeleton, CurrentTime, CurrentPose);
    }

    const Pose* pose = &CurrentPose;
    if (!isFrozen && PreviousState >= 0)
    {
        States[PreviousState].StateClip->Sample(*ModelSkeleton, PreviousTime, PreviousPose);
        Pose::Blend(PreviousPose, CurrentPose, FadeElapsed / FadeSeconds, BlendedPose);
        pose = &BlendedPose;
    }
    Pose::BuildPalette(*ModelSkeleton, *pose, Palette, &JointPositions);
    UpdateBounds();
}

void AnimatorComponent::UpdateBounds()
{
    XMVECTOR minimum = XMVectorReplicate(FLT_MAX);
    XMVECTOR maximum = XMVectorReplicate(-FLT_MAX);
    for (uint32_t i = 0; i < ModelSkeleton->GetJointCount(); i++)
    {
        XMVECTOR position = XMLoadFloat3(&JointPositions[i]);
        XMVECTOR radius = XMVectorReplicate(ModelSkeleton->GetJoint(i).InfluenceRadius);
        minimum = XMVectorMin(minimum, position - radius);
        maximum = XMVectorMax(maximum, position + radius);
    }
    XMVECTOR centre = (minimum + maximum) * 0.5f;
    float sphereRadius = 0.0f;
    for (uint32_t i = 0; i < ModelSkeleton->GetJointCount(); i++)
        sphereRadius = (std::max)(sphereRadius, XMVectorGetX(XMVector3Length(XMLoadFloat3(&JointPositions[i]) - centre)) + ModelSkeleton->GetJoint(i).InfluenceRadius);
    XMStoreFloat4(&ModelBounds, XMVectorSetW(centre, sphereRadius));
}
