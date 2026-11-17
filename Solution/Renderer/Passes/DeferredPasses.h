#pragma once
#include "../RENDERER_API_Macro.h"
#include "../PassGraph.h"

// The standard deferred frame: variance shadow maps, a slim G-buffer and a full-screen lighting
// pass into the back buffer. Built only on the public pass graph and renderer API.
class RENDERER_API DeferredPasses
{
public:
    static void Enlist();
    static void Retire();

    // The G-buffer's albedo (R8G8B8A8, sRGB-encoded) and normal (R16G16, octahedral) images.
    static GraphImageID GetAlbedoImage();
    static GraphImageID GetNormalImage();
};
