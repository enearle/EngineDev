#pragma once
#include "../RENDERER_API_Macro.h"

// The standard deferred frame: variance shadow maps, a slim G-buffer and a full-screen lighting
// pass into the back buffer. Built only on the public pass graph and renderer API.
class RENDERER_API DeferredPasses
{
public:
    static void Enlist();
    static void Retire();
};
