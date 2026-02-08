#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "BufferAllocator.h"

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

class Material
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
    std::vector<uint32_t> TextureHandles;
    std::vector<PreBufferCache*> CachedTextures;
    
public:
    
    Material(std::string name, MaterialFormat materialFormat);
    uint32_t GetTextureHandle(TextureType textureType);
    void LoadMaterial(BufferAllocator* bufferAllocator);
    
};
