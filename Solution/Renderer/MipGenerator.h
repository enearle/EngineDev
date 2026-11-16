#pragma once
#include "RENDERER_API_Macro.h"
#include "RHI/RHIStructures.h"

// Fills the mip chains of uploaded textures on the GPU. A texture is created with every mip and
// storage usage, its upload fills mip 0, and Enqueue schedules the rest. One compute pass, early in
// the frame, drains a few textures per frame. One per program.
//
// Queued images rest in ShaderReadOnly and are returned to it. Supports R8G8B8A8_UNORM; a colour
// image holds sRGB-encoded colour and is averaged as linear light.
class RENDERER_API MipGenerator
{
public:
    static constexpr uint32_t TexturesPerFrame = 4;

    static void Start();
    static void Shutdown();

    static uint32_t FullMipCount(uint32_t width, uint32_t height);
    static void Enqueue(RHIStructures::ImageHandle image, bool isColour = false);
    static void Cancel(RHIStructures::ImageHandle image);
    static uint32_t GetQueueLength();
    static uint32_t GetGeneratedCount();
};
