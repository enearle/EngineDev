#pragma once
#include <cstdint>
#include <vector>
#include "RenderScene.h"

class SceneNode;

// The animated characters of the demo scene: their clips, the crowd of copies (-crowd N) and
// the animation self-checks (-animation-test).
class AnimationDemo
{
public:
    // Every character is animated; the last is the one copied by the crowd and checked by the test.
    static void Start(const std::vector<SceneNode*>& characters, const std::vector<MaterialHandle>& materials, uint32_t crowdCount, bool isAnimationTest);
    static void Update(float dt);
    // After scene extraction: reads back this frame's skinned vertices for the skinning check.
    static void RequestChecks();
    static void PrintResults();
    static void Shutdown();
};
