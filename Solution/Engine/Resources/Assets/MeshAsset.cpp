#include "MeshAsset.h"
#include <filesystem>
#include "../AssetSerializer.h"
#include "RHI/RHIStructures.h"

MeshAsset::MeshAsset(std::vector<Mesh>& meshes, const std::string& name) : GPUAssetBase(name, AssetID::Generate(), {})
{
    uint32_t numUVs = meshes.size();
    Meshes = meshes;
    for (uint32_t i = 0; i < numUVs; ++i)
        Fields.push_back({ResourceType::Material, "Material " + std::to_string(i)});
}

void MeshAsset::Serialize(std::string& data)
{
    DependentAssetBase::Serialize(data);
    
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Meshes.size()));
    
    for (uint32_t i = 0; i < Meshes.size(); ++i)
    {
        AssetSerializer::Write<bool>(data, Meshes[i].IsSkinned());
        AssetSerializer::Write<uint32_t>(data, Meshes[i].GetLocalMaterialIndex());
        AssetSerializer::Write<uint32_t>(data, Meshes[i].GetVertexCount());
        AssetSerializer::Write<uint32_t>(data, Meshes[i].GetIndexCount());
        if (!Meshes[i].IsSkinned())
        {
            AssetSerializer::WriteBytes(data, Meshes[i].GetCachedVertexData(), 
                Meshes[i].GetVertexCount() * sizeof(RHIStructures::Vertex));
            AssetSerializer::WriteBytes(data, Meshes[i].GetCachedIndexData(), 
                Meshes[i].GetIndexCount() * sizeof(uint32_t));
            continue;
        }
        
        // For skinned meshes
        AssetSerializer::WriteBytes(data, Meshes[i].GetCachedVertexData(), 
            Meshes[i].GetVertexCount() * sizeof(RHIStructures::SkinnedVertex));
        AssetSerializer::WriteBytes(data, Meshes[i].GetCachedIndexData(), 
            Meshes[i].GetIndexCount() * sizeof(uint32_t));

        AssetSerializer::Write<uint32_t>(data, Meshes[i].GetBoneTransforms().size());
        std::vector<DirectX::XMFLOAT4X4> boneMatrices(Meshes[i].GetBoneTransforms().size());
        for (uint32_t j = 0; j < Meshes[i].GetBoneTransforms().size(); j++)
        {
            XMStoreFloat4x4(&boneMatrices[j], Meshes[i].GetBoneTransforms()[j]);
        }
        AssetSerializer::WriteBytes(data, boneMatrices.data(), boneMatrices.size() * sizeof(DirectX::XMFLOAT4X4));
        
        std::vector<DirectX::XMFLOAT4X4> boneOffsets(Meshes[i].GetBoneOffsets().size());
        for (uint32_t j = 0; j < Meshes[i].GetBoneOffsets().size(); j++)
        {
            XMStoreFloat4x4(&boneOffsets[j], Meshes[i].GetBoneOffsets()[j]);
        }
        AssetSerializer::WriteBytes(data, boneOffsets.data(), boneOffsets.size() * sizeof(DirectX::XMFLOAT4X4));
        
        // Clear cached data to avoid memory leaks, can get back on load
        Meshes[i].WipeCachedData();
    }
}

void MeshAsset::Deserialize(std::string& data, long& offset)
{
    DependentAssetBase::Deserialize(data, offset);
    
    // TODO: Load mesh data and cache the Vertex/Index data
    
    
}

void MeshAsset::UploadToGPU()
{
    for (uint32_t i = 0; i < Meshes.size(); ++i)
    {
        // TODO: Upload mesh data to GPU
        // use RHI/Uploader.cpp
        
        Meshes[i].WipeCachedData();
    }
}

void MeshAsset::FreeGPUResources()
{
    for (uint32_t i = 0; i < Meshes.size(); ++i)
    {
        // TODO: Free GPU resources for mesh
        // use RHI/Uploader.cpp
    }
}
