#include "MeshAsset.h"
#include <filesystem>
#include "../AssetSerializer.h"
#include "RHI/RHIStructures.h"
#include "../../Animation/Skeleton.h"

MeshAsset::MeshAsset(std::vector<Mesh>& meshes, const std::string& name) : GPUAssetBase(name, AssetID::Generate(), {})
{
    Meshes = std::move(meshes);
    for (uint32_t i = 0; i < Meshes.size(); ++i)
        Fields.push_back({ResourceType::Material, "Material " + std::to_string(i)});
    Fields.push_back({ResourceType::Skeleton, "Skeleton"});
}

void MeshAsset::SetSkeleton(Skeleton* skeleton)
{
    Fields.back().SetID(skeleton->GetID());
}

void MeshAsset::Serialize(std::string& data)
{
    DependentAssetBase::Serialize(data);

    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(PatchCharts.size()));
    for (const PatchChart& chart : PatchCharts)
    {
        AssetSerializer::Write<float>(data, chart.MillimetresU);
        AssetSerializer::Write<float>(data, chart.MillimetresV);
        AssetSerializer::Write<bool>(data, chart.IsPeriodic);
    }
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Meshes.size()));

    for (uint32_t i = 0; i < Meshes.size(); ++i)
    {
        AssetSerializer::Write<bool>(data, Meshes[i].IsSkinned());
        AssetSerializer::Write<uint32_t>(data, Meshes[i].GetLocalMaterialIndex());
        AssetSerializer::WriteString(data, Meshes[i].GetMaterialName());
        const MeshChart& chart = Meshes[i].GetChart();
        AssetSerializer::Write<float>(data, chart.MillimetresU);
        AssetSerializer::Write<float>(data, chart.MillimetresV);
        AssetSerializer::Write<float>(data, chart.MillimetresPerUnit);
        AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(chart.Map.size()));
        AssetSerializer::WriteBytes(data, chart.Map.data(), chart.Map.size());
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
        }
    }
}

void MeshAsset::Deserialize(std::string& data, long& offset)
{
    DependentAssetBase::Deserialize(data, offset);

    PatchCharts.resize(AssetSerializer::Read<uint32_t>(data, offset));
    for (PatchChart& chart : PatchCharts)
    {
        chart.MillimetresU = AssetSerializer::Read<float>(data, offset);
        chart.MillimetresV = AssetSerializer::Read<float>(data, offset);
        chart.IsPeriodic = AssetSerializer::Read<bool>(data, offset);
    }
    uint32_t meshCount = AssetSerializer::Read<uint32_t>(data, offset);

    // DependentAssetBase already populated Fields with IDs only (resized + SetID).
    // Rebuild type/name metadata while preserving the freshly-set IDs.
    if (Fields.size() < meshCount + 1) Fields.resize(meshCount + 1);
    for (uint32_t i = 0; i <= meshCount; ++i)
    {
        AssetID savedId = Fields[i].GetID();
        Fields[i] = i < meshCount ? Field(ResourceType::Material, "Material " + std::to_string(i)) : Field(ResourceType::Skeleton, "Skeleton");
        Fields[i].ID = savedId;
    }

    Meshes.clear();
    Meshes.reserve(meshCount);

    for (uint32_t i = 0; i < meshCount; ++i)
    {
        bool skinned    = AssetSerializer::Read<bool>(data, offset);
        uint32_t matIdx = AssetSerializer::Read<uint32_t>(data, offset);
        std::string materialName = AssetSerializer::ReadString(data, offset);
        MeshChart chart;
        chart.MillimetresU = AssetSerializer::Read<float>(data, offset);
        chart.MillimetresV = AssetSerializer::Read<float>(data, offset);
        chart.MillimetresPerUnit = AssetSerializer::Read<float>(data, offset);
        chart.Map.resize(AssetSerializer::Read<uint32_t>(data, offset));
        AssetSerializer::ReadBytes(data, offset, chart.Map.data(), chart.Map.size());
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
            Meshes.emplace_back(cache, matIdx);
        }
        Meshes.back().SetMaterialName(materialName);
        Meshes.back().SetChart(std::move(chart));
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
    for (Mesh& mesh : Meshes)
        mesh.FreeGPUResources();
}
