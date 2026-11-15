#pragma once
#include "../AssetBase.h"
#include "../Engine/Geometry/Mesh.h"

class Skeleton;

// Fields are for unique materials
// Unique materials represent unique draw calls
// Need a sophisticated batching system to minimize descriptor changes

class MeshAsset : public GPUAssetBase
{
    // Every set of UVs represents a unique mesh
    std::vector<Mesh> Meshes;
public:
    
    // Deserialize Constructor
    MeshAsset() : GPUAssetBase("", {}, {}) {}
    
    // Import Mesh Constructor
    MeshAsset(std::vector<Mesh>& meshes, const std::string& name);

    // Fields hold one material per mesh, then the skeleton the skinned meshes are bound to.
    void SetSkeleton(Skeleton* skeleton);
    const AssetID& GetSkeletonID() const { return Fields.back().GetID(); }
    
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long& offset) override;
    
    virtual void UploadToGPU() override;
    virtual void FreeGPUResources() override;
    
    uint32_t GetMeshCount() const { return Meshes.size(); }
    Mesh& GetMesh(uint32_t index) { return Meshes[index]; }
    
};



