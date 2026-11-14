#pragma once
#include <string>
#include "RENDERER_API_Macro.h"
#include "RenderScene.h"

// Loaded materials, one per name, shared by reference count. Each material is a descriptor set at
// MaterialSetIndex of the renderer's scene layout. One per program.
class RENDERER_API MaterialRegistry
{
public:
    static constexpr uint32_t MaterialSetIndex = 2;

    static MaterialHandle Enlist(const std::string& name);
    static void Retire(MaterialHandle material);
    static bool IsLive(MaterialHandle material);
    static RHIStructures::DescriptorSetHandle GetDescriptorSet(MaterialHandle material);
};
