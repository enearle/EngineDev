#pragma once
#include <string>
#include <vector>
#include "Clip.h"
#include "../Resources/SceneComponent.h"

struct AnimationState
{
    std::string Name;
    const Clip* StateClip = nullptr;
    bool IsLooping = true;
    float Speed = 1.0f;
    std::string Next;                   // Played when a clip that does not loop ends; empty holds the last frame
    float NextFadeSeconds = 0.2f;
};

// Plays the clips of one model's skeleton: named states, fades requested by code and an automatic
// move to the next state when a clip ends. Sits on the model's root node; skinned meshes below it
// take its palette. Enlisted with the AnimationSystem while it exists.
class ENGINE_API AnimatorComponent : public SceneComponentBase
{
    const Skeleton* ModelSkeleton = nullptr;
    std::vector<AnimationState> States;
    int32_t CurrentState = -1;
    int32_t PreviousState = -1;
    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;
    float FadeSeconds = 0.0f;
    float FadeElapsed = 0.0f;

    Pose CurrentPose;
    Pose PreviousPose;
    Pose BlendedPose;
    std::vector<DirectX::XMFLOAT4X4> Palette;
    std::vector<DirectX::XMFLOAT3> JointPositions;
    DirectX::XMFLOAT4 ModelBounds = {};

    void Advance(int32_t state, float& time, float dt, bool isCurrent);
    void UpdateBounds();

public:
    explicit AnimatorComponent(SceneNode* owner);
    ~AnimatorComponent() override;

    SceneComponentType GetType() const override { return SceneComponentType::AnimatorComponentType; }
    void Deserialize(std::string& data, long& offset) override;

    void SetSkeleton(const Skeleton* skeleton);
    const Skeleton* GetSkeleton() const { return ModelSkeleton; }

    void AddState(const AnimationState& state);
    const std::vector<AnimationState>& GetStates() const { return States; }
    const AnimationState* GetCurrentState() const { return CurrentState >= 0 ? &States[CurrentState] : nullptr; }
    float GetCurrentTime() const { return CurrentTime; }
    bool IsFading() const { return PreviousState >= 0; }

    // Starts a state from its beginning, fading from the current one over fadeSeconds. False when no
    // state has that name.
    bool Play(const std::string& state, float fadeSeconds);
    void SetCurrentTime(float seconds) { CurrentTime = seconds; }

    // Advances the states and rebuilds the palette. A frozen animator samples its state at time 0.
    void Update(float dt, bool isFrozen);

    // Skinning matrices, one per joint, from the last Update.
    const std::vector<DirectX::XMFLOAT4X4>& GetPalette() const { return Palette; }
    // A model-space sphere (centre, radius) holding every joint grown by its influence radius.
    const DirectX::XMFLOAT4& GetModelBounds() const { return ModelBounds; }
};
