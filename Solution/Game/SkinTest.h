#pragma once
#include <string>

class Camera;
class SceneNode;

// -skin-test: a scripted zoom on procedural skin that stops at a few view widths and checks the
// Phase 5 gate at each: the G-buffer from the wanted page level against the same view forced two
// levels coarser (level invariance), memory, G-buffer GPU time, and frames to a resident view. The
// game quits after the last stop and the results are printed at exit. -skin-test plane puts the
// material on a plane as a chart 1 m wide.
class SkinTest
{
public:
    static void StartOnPlane();
    // Before the character is uploaded: its skin vertices pick where the camera looks.
    static void StartOnCharacter(SceneNode* character);
    // -skin-stop N: no checks; the camera stays at stop N (from 0) for looking at.
    static void HoldAtStop(int stop);
    static void Update(float dt, Camera* camera);
    static void PrintResults();
    static void Shutdown();
};
