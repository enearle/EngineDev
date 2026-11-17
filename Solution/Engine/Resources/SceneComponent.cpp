#include "SceneComponent.h"
#include <algorithm>
#include "AssetSerializer.h"
#include "../Scene/SceneNode.h"
#include "../Animation/AnimatorComponent.h"
#include <map>
#include "MaterialRegistry.h"
#include "Skinning.h"
#include "Skin/Skin.h"

// Every procedural_skin material reads the band table of this name.
static const char* SkinBandTable = "skin";

struct SharedChart
{
    PagedTextureID Chart = 0;
    uint32_t References = 0;
};

// One chart per submesh of a mesh asset, shared by every component that draws it, so copies of a
// character share pages.
static std::map<std::pair<const MeshAsset*, uint32_t>, SharedChart> SharedCharts;

static PagedTextureID AcquireChart(MeshAsset* asset, uint32_t submesh)
{
    SharedChart& shared = SharedCharts[{ asset, submesh }];
    if (shared.References++ == 0)
    {
        const MeshChart& chart = asset->GetMesh(submesh).GetChart();
        shared.Chart = Skin::EnlistChart({ .MillimetresU = chart.MillimetresU, .MillimetresV = chart.MillimetresV,
                                           .BandTable = SkinBandTable, .Map = chart.Map });
    }
    return shared.Chart;
}

static void ReleaseChart(const MeshAsset* asset, uint32_t submesh)
{
    auto found = SharedCharts.find({ asset, submesh });
    if (found == SharedCharts.end() || --found->second.References > 0)
        return;
    Skin::RetireChart(found->second.Chart);
    SharedCharts.erase(found);
}

struct SharedPatchTable
{
    uint32_t Table = 0;
    std::vector<PagedTextureID> Charts;
    uint32_t References = 0;
};

// The seam patch charts of a mesh asset and the table naming them, shared like its charts.
static std::map<const MeshAsset*, SharedPatchTable> SharedPatches;

static uint32_t AcquirePatchTable(MeshAsset* asset)
{
    SharedPatchTable& shared = SharedPatches[asset];
    if (shared.References++ == 0)
    {
        for (const PatchChart& patch : asset->GetPatchCharts())
            shared.Charts.push_back(Skin::EnlistChart({ .MillimetresU = patch.MillimetresU, .MillimetresV = patch.MillimetresV,
                                                        .IsPeriodic = patch.IsPeriodic, .BandTable = SkinBandTable }));
        shared.Table = Skin::EnlistPatchTable(shared.Charts);
    }
    return shared.Table;
}

static void ReleasePatchTable(const MeshAsset* asset)
{
    auto found = SharedPatches.find(asset);
    if (found == SharedPatches.end() || --found->second.References > 0)
        return;
    Skin::RetirePatchTable(found->second.Table);
    for (PagedTextureID chart : found->second.Charts)
        Skin::RetireChart(chart);
    SharedPatches.erase(found);
}

void SceneComponentBase::Serialize(std::string& data)
{
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Fields.size()));
    for (const Field& field : Fields)
    {
        AssetID id = field.GetID();
        AssetSerializer::WriteBytes(data, &id, sizeof(AssetID));
    }
}

void SceneComponentBase::Deserialize(std::string& data, long& offset)
{
    uint32_t count = AssetSerializer::Read<uint32_t>(data, offset);
    uint32_t consume = std::min<uint32_t>(count, static_cast<uint32_t>(Fields.size()));
    for (uint32_t i = 0; i < consume; ++i)
    {
        AssetID id;
        AssetSerializer::ReadBytes(data, offset, &id, sizeof(AssetID));
        Fields[i].ID = id;
    }

    // Advance past any extra serialized IDs we couldn't store.
    if (count > consume)
        offset += static_cast<long>((count - consume) * sizeof(AssetID));
}

void MeshComponent::Deserialize(std::string& data, long& offset)
{
    SceneComponentBase::Deserialize(data, offset);

    // Load mesh if one is selected
    if (Fields[0].GetID().IsValid())
    {
        LoadedMesh = static_cast<MeshAsset*>(ResourceManager::GetAsset(Fields[0].GetID(), ResourceType::Mesh));
    }

}

MeshAsset* MeshComponent::GetMeshAsset()
{
    if (LoadedMesh) return LoadedMesh;

    LoadedMesh = static_cast<MeshAsset*>(ResourceManager::GetAsset(Fields[0].GetID(), ResourceType::Mesh));
    if (!LoadedMesh) throw std::runtime_error("Mesh asset not found.");

    return LoadedMesh;
}

void MeshComponent::AssignMaterials(const std::vector<MaterialHandle>& materialsByLocalIndex)
{
    MeshAsset* meshAsset = GetMeshAsset();
    SubmeshMaterials.resize(meshAsset->GetMeshCount());
    for (uint32_t i = 0; i < meshAsset->GetMeshCount(); i++)
    {
        uint32_t localIndex = meshAsset->GetMesh(i).GetLocalMaterialIndex();
        SubmeshMaterials[i] = localIndex < materialsByLocalIndex.size() ? materialsByLocalIndex[localIndex] : MaterialHandle{};
    }
}

void MeshComponent::AssignMaterialsByName()
{
    MeshAsset* meshAsset = GetMeshAsset();
    for (MaterialHandle material : EnlistedMaterials)
        MaterialRegistry::Retire(material);
    EnlistedMaterials.clear();
    for (uint32_t submesh : HeldCharts)
        ReleaseChart(meshAsset, submesh);
    HeldCharts.clear();
    if (IsHoldingPatchTable)
        ReleasePatchTable(meshAsset);
    IsHoldingPatchTable = false;

    MaterialTypeID skinType = 0;
    bool hasSkinType = MaterialTypes::Find("procedural_skin", skinType);
    SubmeshMaterials.resize(meshAsset->GetMeshCount());
    for (uint32_t i = 0; i < meshAsset->GetMeshCount(); i++)
    {
        SubmeshMaterials[i] = MaterialRegistry::Enlist(meshAsset->GetMesh(i).GetMaterialName());
        EnlistedMaterials.push_back(SubmeshMaterials[i]);

        // A procedural surface is bound to the chart of the submesh it is assigned to.
        bool isSkin = hasSkinType && MaterialRegistry::GetType(SubmeshMaterials[i]) == skinType;
        if (isSkin && meshAsset->GetMesh(i).GetChart().MillimetresU > 0.0f)
        {
            float chart = static_cast<float>(AcquireChart(meshAsset, i));
            MaterialRegistry::SetParameter(SubmeshMaterials[i], "chart", { &chart, 1 });
            HeldCharts.push_back(i);
            if (!meshAsset->GetPatchCharts().empty())
            {
                if (!IsHoldingPatchTable)
                    PatchTable = AcquirePatchTable(meshAsset);
                IsHoldingPatchTable = true;
                float table = static_cast<float>(PatchTable);
                MaterialRegistry::SetParameter(SubmeshMaterials[i], "patch_table", { &table, 1 });
            }
        }
    }
}

AnimatorComponent* MeshComponent::GetAnimator()
{
    if (IsAnimatorSearched)
        return Animator;
    IsAnimatorSearched = true;
    for (SceneNode* node = Owner; node && !Animator; node = node->GetParent())
        Animator = static_cast<AnimatorComponent*>(node->GetComponent(AnimatorComponentType));
    return Animator;
}

SkinnedMeshHandle MeshComponent::GetSkinnedMesh(uint32_t submesh)
{
    if (SkinnedMeshes.size() <= submesh)
        SkinnedMeshes.resize(submesh + 1);
    if (!SkinnedMeshes[submesh].IsValid())
        SkinnedMeshes[submesh] = Skinning::Enlist(GetMeshAsset()->GetMesh(submesh).GetGPUMesh());
    return SkinnedMeshes[submesh];
}

void MeshComponent::FreeGPUResources()
{
    for (SkinnedMeshHandle mesh : SkinnedMeshes)
        Skinning::Retire(mesh);
    SkinnedMeshes.clear();
    for (MaterialHandle material : EnlistedMaterials)
        MaterialRegistry::Retire(material);
    EnlistedMaterials.clear();
    SubmeshMaterials.clear();
    for (uint32_t submesh : HeldCharts)
        ReleaseChart(LoadedMesh, submesh);
    HeldCharts.clear();
    if (IsHoldingPatchTable)
        ReleasePatchTable(LoadedMesh);
    IsHoldingPatchTable = false;
}

bool SceneComponentFactory::SceneComponentExists(SceneComponentType sceneComponentType, SceneNode* owner)
{
    if (owner == nullptr)
        return false;

    for (SceneComponentBase* component : owner->GetComponents())
    {
        if (component->GetType() == sceneComponentType)
            return true;
    }
    return false;
}

SceneComponentBase* SceneComponentFactory::SceneComponentBuilder(SceneComponentType sceneComponentType,
    SceneNode* owner)
{
    switch (sceneComponentType)
    {
    case SceneComponentType::MeshComponentType:
        return new MeshComponent(owner);
    case SceneComponentType::AnimatorComponentType:
        return new AnimatorComponent(owner);
    default:
        return nullptr;
    }
}
