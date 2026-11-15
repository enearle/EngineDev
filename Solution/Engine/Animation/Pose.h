#pragma once
#include <vector>
#include "Skeleton.h"

// One local transform per joint of a skeleton.
class ENGINE_API Pose
{
public:
    std::vector<JointTransform> Joints;

    void SetToBind(const Skeleton& skeleton);

    // out = a at weight 0, b at weight 1. Rotations by normalised lerp on the shorter arc.
    static void Blend(const Pose& a, const Pose& b, float weight, Pose& out);

    // Skinning matrices, InverseBind * Model per joint. jointPositions, when given, receives each
    // joint's model-space position.
    static void BuildPalette(const Skeleton& skeleton, const Pose& pose, std::vector<DirectX::XMFLOAT4X4>& out,
                             std::vector<DirectX::XMFLOAT3>* jointPositions = nullptr);
};

DirectX::XMVECTOR ENGINE_API NlerpShortest(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, float weight);
