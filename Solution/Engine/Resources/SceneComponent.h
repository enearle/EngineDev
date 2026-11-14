#pragma once
#include <stdexcept>
#include <vector>
#include "Field.h"
#include "Assets/MeshAsset.h"

class SceneNode;

enum ENGINE_API SceneComponentType : uint32_t
{
    MeshComponentType,
    // RigidBody,
    // BoxCollider,
    // etc.
    Count
};

static SceneComponentType ComponentTypeFromInt(uint32_t type)
{
    if (type < static_cast<uint32_t>(SceneComponentType::Count))
        return static_cast<SceneComponentType>(type);
    throw std::invalid_argument("Invalid component type");
}

// Scene components cannot be duplicated on the same scene node
class ENGINE_API SceneComponentBase
{
protected:
    std::vector<Field> Fields;
    SceneNode* Owner;
    SceneComponentBase(std::vector<Field> fields, SceneNode* owner) : Fields(std::move(fields)), Owner(owner) {}
    virtual ~SceneComponentBase() = default;
    
public:
    virtual SceneComponentType GetType() const = 0;
    virtual void Serialize(std::string& data);
    virtual void Deserialize(std::string& data, long& offset);
};

class ENGINE_API MeshComponent : public SceneComponentBase
{
    MeshAsset* LoadedMesh = nullptr;
    std::vector<MaterialHandle> SubmeshMaterials;
    std::vector<DirectX::XMFLOAT4X4> BonePalette;
public:
    // Create single field for MeshAsset
    MeshComponent(SceneNode* owner) : SceneComponentBase({Field(ResourceType::Mesh, "MeshAsset")}, owner) { }

    virtual SceneComponentType GetType() const override { return SceneComponentType::MeshComponentType; }
    virtual void Deserialize(std::string& data, long& offset) override;
    
    void SetMeshAsset(MeshAsset* meshAsset) { LoadedMesh = meshAsset; Fields[0].SetID(meshAsset->GetID());}
    MeshAsset* GetMeshAsset();

    // Materials are listed by the mesh's local material index and stored per submesh.
    void AssignMaterials(const std::vector<MaterialHandle>& materialsByLocalIndex);
    MaterialHandle GetSubmeshMaterial(uint32_t submesh) const { return submesh < SubmeshMaterials.size() ? SubmeshMaterials[submesh] : MaterialHandle{}; }

    // Model-space skinning matrices for a skinned mesh, written by animation each frame.
    std::vector<DirectX::XMFLOAT4X4>& GetBonePalette() { return BonePalette; }
};

// Scene components are responsible for managing their own resources and lifecycle
class ENGINE_API SceneComponentFactory
{
public:
    static bool SceneComponentExists(SceneComponentType sceneComponentType, SceneNode* owner);
    static SceneComponentBase* SceneComponentBuilder(SceneComponentType sceneComponentType, SceneNode* owner);
};