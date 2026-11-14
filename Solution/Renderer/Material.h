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
    RHIStructures::DescriptorSetHandle LoadMaterial(uint32_t pipelineIndex, uint32_t setIndex);

};
