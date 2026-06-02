#include "MeshAsset.h"
#include <filesystem>
#include "../AssetSerializer.h"
#include "RHI/RHIStructures.h"

MeshAsset::MeshAsset(std::vector<Mesh>& meshes, const std::string& name) : GPUAssetBase(name, AssetID::Generate(), {})
{
    Meshes = std::move(meshes);
    for (uint32_t i = 0; i < Meshes.size(); ++i)
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
        }
        else
        {
            AssetSerializer::WriteBytes(data, Meshes[i].GetCachedVertexData(),
                Meshes[i].GetVertexCount() * sizeof(RHIStructures::SkinnedVertex));
            AssetSerializer::WriteBytes(data, Meshes[i].GetCachedIndexData(),
                Meshes[i].GetIndexCount() * sizeof(uint32_t));

            const auto& boneTransforms = Meshes[i].GetBoneTransforms();
            const auto& boneOffsets    = Meshes[i].GetBoneOffsets();
            uint32_t boneCount = static_cast<uint32_t>(boneTransforms.size());
            AssetSerializer::Write<uint32_t>(data, boneCount);

            std::vector<DirectX::XMFLOAT4X4> bT(boneCount);
            for (uint32_t j = 0; j < boneCount; ++j) XMStoreFloat4x4(&bT[j], boneTransforms[j]);
            AssetSerializer::WriteBytes(data, bT.data(), boneCount * sizeof(DirectX::XMFLOAT4X4));

            std::vector<DirectX::XMFLOAT4X4> bO(boneCount);
            for (uint32_t j = 0; j < boneCount; ++j) XMStoreFloat4x4(&bO[j], boneOffsets[j]);
            AssetSerializer::WriteBytes(data, bO.data(), boneCount * sizeof(DirectX::XMFLOAT4X4));
        }
        
        Meshes[i].WipeCachedData();
    }
}

void MeshAsset::Deserialize(std::string& data, long& offset)
{
    DependentAssetBase::Deserialize(data, offset);

    uint32_t meshCount = AssetSerializer::Read<uint32_t>(data, offset);

    // DependentAssetBase already populated Fields with IDs only (resized + SetID).
    // Rebuild type/name metadata while preserving the freshly-set IDs.
    if (Fields.size() < meshCount) Fields.resize(meshCount);
    for (uint32_t i = 0; i < meshCount; ++i)
    {
        AssetID savedId = Fields[i].GetID();
        Fields[i] = Field(ResourceType::Material, "Material " + std::to_string(i));
        Fields[i].ID = savedId;
    }

    Meshes.clear();
    Meshes.reserve(meshCount);

    for (uint32_t i = 0; i < meshCount; ++i)
    {
        bool skinned    = AssetSerializer::Read<bool>(data, offset);
        uint32_t matIdx = AssetSerializer::Read<uint32_t>(data, offset);
        uint32_t vCount = AssetSerializer::Read<uint32_t>(data, offset);
        uint32_t iCount = AssetSerializer::Read<uint32_t>(data, offset);

        if (!skinned)
        {
            VertexCache* cache = new VertexCache();
            cache->Vertices.resize(vCount);
            cache->Indices.resize(iCount);
            AssetSerializer::ReadBytes(data, offset, cache->Vertices.data(),
                vCount * sizeof(RHIStructures::Vertex));
            AssetSerializer::ReadBytes(data, offset, cache->Indices.data(),
                iCount * sizeof(uint32_t));
            Meshes.emplace_back(cache, matIdx);
        }
        else
        {
            SkinnedVertexCache* cache = new SkinnedVertexCache();
            cache->Vertices.resize(vCount);
            cache->Indices.resize(iCount);
            AssetSerializer::ReadBytes(data, offset, cache->Vertices.data(),
                vCount * sizeof(RHIStructures::SkinnedVertex));
            AssetSerializer::ReadBytes(data, offset, cache->Indices.data(),
                iCount * sizeof(uint32_t));

            uint32_t boneCount = AssetSerializer::Read<uint32_t>(data, offset);
            std::vector<DirectX::XMFLOAT4X4> bT(boneCount);
            std::vector<DirectX::XMFLOAT4X4> bO(boneCount);
            AssetSerializer::ReadBytes(data, offset, bT.data(),
                boneCount * sizeof(DirectX::XMFLOAT4X4));
            AssetSerializer::ReadBytes(data, offset, bO.data(),
                boneCount * sizeof(DirectX::XMFLOAT4X4));

            std::vector<DirectX::XMMATRIX> bTx(boneCount), bOx(boneCount);
            for (uint32_t j = 0; j < boneCount; ++j)
            {
                bTx[j] = XMLoadFloat4x4(&bT[j]);
                bOx[j] = XMLoadFloat4x4(&bO[j]);
            }
            // Serialize order: transforms before offsets. Mesh ctor takes (offsets, transforms).
            Meshes.emplace_back(cache, matIdx, bOx, bTx);
        }
    }
}

void MeshAsset::UploadToGPU()
{
    for (Mesh& mesh : Meshes)
    {
        mesh.UploadToGPU();
        mesh.WipeCachedData();
    }
}

void MeshAsset::FreeGPUResources()
{
    // D7: Uploader has no public release API yet. Stubbed.
}
