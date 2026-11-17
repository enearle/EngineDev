#pragma once
#include <stdexcept>
#include <vector>
#include "Field.h"
#include "Assets/MeshAsset.h"

class SceneNode;
class AnimatorComponent;

enum ENGINE_API SceneComponentType : uint32_t
{
    MeshComponentType,
    AnimatorComponentType,
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

public:
    virtual ~SceneComponentBase() = default;
    SceneNode* GetOwner() const { return Owner; }
    virtual SceneComponentType GetType() const = 0;
    virtual void Serialize(std::string& data);
    virtual void Deserialize(std::string& data, long& offset);
};

class ENGINE_API MeshComponent : public SceneComponentBase
{
    MeshAsset* LoadedMesh = nullptr;
    std::vector<MaterialHandle> SubmeshMaterials;
    std::vector<MaterialHandle> EnlistedMaterials;  // Held by this component; retired with its GPU resources
    std::vector<uint32_t> HeldCharts;               // Submeshes whose skin chart this component holds
    uint32_t PatchTable = 0;                        // The mesh asset's seam patch table, while held
    bool IsHoldingPatchTable = false;
    std::vector<SkinnedMeshHandle> SkinnedMeshes;     // Per submesh; enlisted on first use
    AnimatorComponent* Animator = nullptr;
    bool IsAnimatorSearched = false;
public:
    // Create single field for MeshAsset
    MeshComponent(SceneNode* owner) : SceneComponentBase({Field(ResourceType::Mesh, "MeshAsset")}, owner) { }
    ~MeshComponent() override { FreeGPUResources(); }

    virtual SceneComponentType GetType() const override { return SceneComponentType::MeshComponentType; }
    virtual void Deserialize(std::string& data, long& offset) override;
    
    void SetMeshAsset(MeshAsset* meshAsset) { LoadedMesh = meshAsset; Fields[0].SetID(meshAsset->GetID());}
    MeshAsset* GetMeshAsset();

    // Materials are listed by the mesh's local material index and stored per submesh.
    void AssignMaterials(const std::vector<MaterialHandle>& materialsByLocalIndex);
    // Each submesh gets the material named by its mesh, enlisted here and retired with this component.
    // A procedural_skin material is bound to the submesh's chart, which copies of the mesh share.
    void AssignMaterialsByName();
    bool IsAssignedByName() const { return !EnlistedMaterials.empty(); }
    MaterialHandle GetSubmeshMaterial(uint32_t submesh) const { return submesh < SubmeshMaterials.size() ? SubmeshMaterials[submesh] : MaterialHandle{}; }

    // The animator on this node or its nearest ancestor that has one, found on first use. Skinned
    // meshes take their palette and world matrix from it.
    AnimatorComponent* GetAnimator();

    // This component's own skinned copy of a skinned submesh, enlisted with the renderer's skinning
    // on first use. Copies of a character share the mesh asset but not these.
    SkinnedMeshHandle GetSkinnedMesh(uint32_t submesh);
    void FreeGPUResources();
};

// Scene components are responsible for managing their own resources and lifecycle
class ENGINE_API SceneComponentFactory
{
public:
    static bool SceneComponentExists(SceneComponentType sceneComponentType, SceneNode* owner);
    static SceneComponentBase* SceneComponentBuilder(SceneComponentType sceneComponentType, SceneNode* owner);
};