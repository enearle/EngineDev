#include "Material.h"

#include "BufferAllocator.h"
#include "RHIConstants.h"
#include "RHIStructures.h"
#include "Image/ImageImport.h"

using namespace RHIStructures;
using namespace RHIConstants;

PreBufferCache::~PreBufferCache()
{
    // TODO handle cleanup/structure better (2 pointers to Data.Pixels)
    delete ImportHandle;
    delete Desc;
}

Material::Material(std::string name, MaterialFormat materialFormat) : Name(name), Format(materialFormat)
{
    PreBufferCache* diffuseAlphaCache = nullptr;
    PreBufferCache* normalCache = nullptr;
    PreBufferCache* metallnessRoughnessAOCache = nullptr;
    PreBufferCache* emissiveCache = nullptr;
    std::vector<std::string> metalRoughAOMaskPaths;
    
    std::string texturePath = "Textures/" + name;
    switch (materialFormat)
    {
    case PBR:
        
        diffuseAlphaCache = new PreBufferCache();
        diffuseAlphaCache->ImportHandle = new ImageImport(texturePath + "_diff");
        diffuseAlphaCache->Desc = new ImageDesc(DefaultTextureDesc);
        diffuseAlphaCache->Desc->Width = diffuseAlphaCache->ImportHandle->GetData()->Width;
        diffuseAlphaCache->Desc->Height = diffuseAlphaCache->ImportHandle->GetData()->Height;
        diffuseAlphaCache->Desc->Size = diffuseAlphaCache->ImportHandle->GetData()->TotalSize;
        diffuseAlphaCache->Desc->InitialData = diffuseAlphaCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(diffuseAlphaCache);
        
        normalCache = new PreBufferCache();
        normalCache->ImportHandle = new ImageImport(texturePath + "_norm");
        normalCache->Desc = new ImageDesc(DefaultTextureDesc);
        normalCache->Desc->Width = normalCache->ImportHandle->GetData()->Width;
        normalCache->Desc->Height = normalCache->ImportHandle->GetData()->Height;
        normalCache->Desc->Size = normalCache->ImportHandle->GetData()->TotalSize;
        normalCache->Desc->InitialData = normalCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(normalCache);
        
        metalRoughAOMaskPaths = { texturePath + "_metal", texturePath + "_rough", texturePath + "_ao" };
        
        metallnessRoughnessAOCache = new PreBufferCache();
        metallnessRoughnessAOCache->ImportHandle = new ImageImport(metalRoughAOMaskPaths, DefaultMetalnessRoughnessOcclusion);
        metallnessRoughnessAOCache->Desc = new ImageDesc(DefaultTextureDesc);
        metallnessRoughnessAOCache->Desc->Width = metallnessRoughnessAOCache->ImportHandle->GetData()->Width;
        metallnessRoughnessAOCache->Desc->Height = metallnessRoughnessAOCache->ImportHandle->GetData()->Height;
        metallnessRoughnessAOCache->Desc->Size = metallnessRoughnessAOCache->ImportHandle->GetData()->TotalSize;
        metallnessRoughnessAOCache->Desc->InitialData = metallnessRoughnessAOCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(metallnessRoughnessAOCache);

        break;
        
    case PBREmissive:
        
        diffuseAlphaCache = new PreBufferCache();
        diffuseAlphaCache->ImportHandle = new ImageImport(texturePath + "_diff");
        diffuseAlphaCache->Desc = new ImageDesc(DefaultTextureDesc);
        diffuseAlphaCache->Desc->Width = diffuseAlphaCache->ImportHandle->GetData()->Width;
        diffuseAlphaCache->Desc->Height = diffuseAlphaCache->ImportHandle->GetData()->Height;
        diffuseAlphaCache->Desc->Size = diffuseAlphaCache->ImportHandle->GetData()->TotalSize;
        diffuseAlphaCache->Desc->InitialData = diffuseAlphaCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(diffuseAlphaCache);
        
        normalCache = new PreBufferCache();
        normalCache->ImportHandle = new ImageImport(texturePath + "_norm");
        normalCache->Desc = new ImageDesc(DefaultTextureDesc);
        normalCache->Desc->Width = normalCache->ImportHandle->GetData()->Width;
        normalCache->Desc->Height = normalCache->ImportHandle->GetData()->Height;
        normalCache->Desc->Size = normalCache->ImportHandle->GetData()->TotalSize;
        normalCache->Desc->InitialData = normalCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(normalCache);
        
        metalRoughAOMaskPaths = { texturePath + "_metal", texturePath + "_rough", texturePath + "_ao" };
        
        metallnessRoughnessAOCache = new PreBufferCache();
        metallnessRoughnessAOCache->ImportHandle = new ImageImport(metalRoughAOMaskPaths, DefaultMetalnessRoughnessOcclusion);
        metallnessRoughnessAOCache->Desc = new ImageDesc(DefaultTextureDesc);
        metallnessRoughnessAOCache->Desc->Width = metallnessRoughnessAOCache->ImportHandle->GetData()->Width;
        metallnessRoughnessAOCache->Desc->Height = metallnessRoughnessAOCache->ImportHandle->GetData()->Height;
        metallnessRoughnessAOCache->Desc->Size = metallnessRoughnessAOCache->ImportHandle->GetData()->TotalSize;
        metallnessRoughnessAOCache->Desc->InitialData = metallnessRoughnessAOCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(metallnessRoughnessAOCache);
        
        emissiveCache = new PreBufferCache();
        emissiveCache->ImportHandle = new ImageImport(texturePath + "_emissive");
        emissiveCache->Desc = new ImageDesc(DefaultTextureDesc);
        emissiveCache->Desc->Width = emissiveCache->ImportHandle->GetData()->Width;
        emissiveCache->Desc->Height = emissiveCache->ImportHandle->GetData()->Height;
        emissiveCache->Desc->Size = emissiveCache->ImportHandle->GetData()->TotalSize;
        emissiveCache->Desc->InitialData = emissiveCache->ImportHandle->GetData()->Pixels;
        CachedTextures.push_back(emissiveCache);
        
        break;
    
    default:
        throw std::runtime_error("Invalid MaterialFormat");       
    }
}

uint32_t Material::GetTextureHandle(TextureType textureType)
{
    switch (Format)
    {
    case PBR:
        switch (textureType)
        {
        case DiffuseAlpha:
            return TextureHandles[0];
        case Normal:
            return TextureHandles[1];
        case MetalnessRoughnessOcclusion:
            return TextureHandles[2];
        default:
            throw std::runtime_error("Invalid TextureType");       
        }
    case PBREmissive:
        switch (textureType)
        {
        case DiffuseAlpha:
            return TextureHandles[0];
        case Normal:
            return TextureHandles[1];
        case MetalnessRoughnessOcclusion:
            return TextureHandles[2];
        case Emissive:
            return TextureHandles[3];
        default:
            throw std::runtime_error("Invalid TextureType");       
        }
        
    default:
        throw std::runtime_error("Invalid MaterialFormat");      
    }
}

uint64_t Material::LoadMaterial(uint32_t pipelineIndex)
{
    BufferAllocator* bufferAllocator = BufferAllocator::GetInstance();
    std::vector<DescriptorSetBinding> bindings;
    bindings.reserve(CachedTextures.size());   
    for (uint32_t i = 0; i < CachedTextures.size(); ++i)
    {
        bindings.emplace_back(DescriptorSetBinding {
            .Binding = i,
            .ResourceID = bufferAllocator->CreateImage(*CachedTextures[i]->Desc)
        });
        
        delete CachedTextures[i];
    }
    CachedTextures.clear();
    
    return bufferAllocator->AllocateDescriptorSet(pipelineIndex, bindings);
}

