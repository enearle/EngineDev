#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"

using MaterialTypeID = uint32_t;

struct MaterialField
{
    std::string Name;
    uint32_t Components = 1;            // 1 to 4 floats
    std::vector<float> Default;         // Colours are linear
    uint32_t Offset = 0;                // Bytes into the parameter block; set by Register with HLSL cbuffer packing
};

struct MaterialTextureSlot
{
    std::string Name;
    bool IsColour = false;              // Holds sRGB-encoded colour
};

// A surface shader, a parameter block and texture slots. The G-buffer shader is ps_gbuffer_<Name>,
// built from Shaders/Surfaces/<Name>.hlsli, whose cbuffer and textures must match Fields and Textures.
struct MaterialTypeDesc
{
    std::string Name;
    std::vector<MaterialField> Fields;
    std::vector<MaterialTextureSlot> Textures;      // Material set slots 1 onwards, in order
};

// Material types are content of the build: registered at startup, before the G-buffer pass is
// enlisted, and fixed afterwards. One per program.
class RENDERER_API MaterialTypes
{
public:
    static constexpr uint32_t MaxTypes = 8;             // The draw list's batch key holds 3 bits of type
    static constexpr uint32_t ParameterBlockSize = 256;
    static constexpr uint32_t MaxTextures = 8;

    static MaterialTypeID Register(MaterialTypeDesc desc);
    static void RegisterBuiltInTypes();
    static void Clear();

    static uint32_t GetCount();
    static const MaterialTypeDesc& Get(MaterialTypeID type);
    static bool Find(const std::string& name, MaterialTypeID& type);
};
