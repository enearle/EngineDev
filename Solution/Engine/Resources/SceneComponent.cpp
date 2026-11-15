#include "SceneComponent.h"
#include <algorithm>
#include "AssetSerializer.h"
#include "../Scene/SceneNode.h"
#include "../Animation/AnimatorComponent.h"
#include "Skinning.h"

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
