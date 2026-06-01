#pragma once
#include "../AssetBase.h"
#include "../Engine/Geometry/Mesh.h"

// Fields are for unique materials
// Unique materials represent unique draw calls
// Need a sophisticated batching system to minimize descriptor changes

class MeshAsset : public GPUAssetBase
{
    std::vector<Mesh> Meshes;
public:
    
    // Deserialize Constructor
    MeshAsset();
    
    // Import Mesc Constructor
    MeshAsset(std::vector<Mesh>& meshes, const std::string& name);
    
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long& offset) override;
    
    virtual void UploadToGPU() override;
    virtual void FreeGPUResources() override;
    
};



