#pragma once
#include <DirectXMath.h>
#include <vector>
#include "ENGINE_API_Macro.h"

class SceneNode;
class MeshComponent;

// Animates three bones of the Derp test mesh by hand, writing the same palette to every skinned
// mesh component under the root. Replaced by clips and an animator in Phase 3.
class ENGINE_API DerpMover
{
    std::vector<MeshComponent*> SkinnedComponents;
    std::vector<DirectX::XMMATRIX> BoneOffsets;
    std::vector<DirectX::XMFLOAT4X4> Palette;
    bool DanceForward = true;
    float ProgressMove = 0.0f;
    float DurationMove = 3.0f;
    float ProgressDance = 0.0f;

public:
    explicit DerpMover(SceneNode* derpRoot);
    void MoveTheDerp(double dt);
};
