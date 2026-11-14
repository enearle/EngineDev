#pragma once
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "RHI/RHIStructures.h"

namespace RHIStructures
{
    struct ImageDesc;
}

class ImageImport;


struct PreBufferCache
{
    ImageImport* ImportHandle;
    RHIStructures::ImageDesc* Desc;

    ~PreBufferCache();
};

struct LoadedMaterial
{
    RHIStructures::DescriptorSetHandle Set = {};
    std::vector<RHIStructures::ImageHandle> Images;
};

class RENDERER_API Material
{
public:
    enum MaterialFormat
    {
        PBR,
        PBREmissive
    };

    enum TextureType
    {
        DiffuseAlpha,
        Normal,
        MetalnessRoughnessOcclusion,
        Emissive
    };

private:

    std::string Name;
    MaterialFormat Format;
    std::vector<PreBufferCache*> CachedTextures;

public:

    Material(std::string name, MaterialFormat materialFormat);
    LoadedMaterial LoadMaterial(uint32_t layoutID, uint32_t setIndex);

};
