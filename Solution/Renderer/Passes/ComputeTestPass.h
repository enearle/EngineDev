#pragma once
#include "../RENDERER_API_Macro.h"

// The Phase 2 gate: a compute pass writes an image whose texels are a function of the frame number,
// a raster pass draws it into a corner of the back buffer, and one row a frame is read back and
// checked against the same function. Retire prints the result. Built only on the public graph and
// readback API.
class RENDERER_API ComputeTestPass
{
public:
    static void Enlist();
    static void Retire();
};
