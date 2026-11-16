#pragma once
#include <span>
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "MaterialType.h"
#include "RenderScene.h"

class PipelineExecutor;

// Loaded materials, one per name, shared by reference count. One per program.
//
// Every material type shares one material set at MaterialSetIndex of the renderer's scene layout:
// slot 0 is the parameter block, a dynamic uniform buffer in the frame ring, and slots 1 to 8 are
// sampled images, the unused ones holding a 1 x 1 white image. Parameters are copied into the ring
// for each material used in a frame, so they can change at any time.
class RENDERER_API MaterialRegistry
{
public:
    static constexpr uint32_t MaterialSetIndex = 2;
    static constexpr uint32_t FirstTextureSlot = 1;

    static std::vector<RHIStructures::DescriptorBinding> GetMaterialSetBindings();
    static void Start();
    static void Shutdown();

    // Reads Engine/Materials/<name>.material. Without one it falls back to textured_pbr from the
    // texture file-name convention, and without those textures to a flat grey material.
    static MaterialHandle Enlist(const std::string& name);
    static void Retire(MaterialHandle material);
    static bool IsLive(MaterialHandle material);
    static MaterialTypeID GetType(MaterialHandle material);
    static void SetParameter(MaterialHandle material, const std::string& field, std::span<const float> values);

    // Binds the material set, copying the parameter block into the frame ring on first use in the frame.
    static void Bind(PipelineExecutor* executor, MaterialHandle material);
};
