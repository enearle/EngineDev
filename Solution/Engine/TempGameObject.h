#pragma once
#include "EngineConstants.h"
#include "RHI/Geometry/Mesh.h"

class TempGameObject
{
    
    std::vector<uint64_t> Materials;
    EngineConstants::ModelData ModelData;
    std::vector<EngineConstants::ModelData> ModelDataArray;
    RootNode MeshRoot;
    uint64_t geometryVPDescriptorSet;
    uint64_t lightingVPDescriptorSet;
    uint64_t BoneDescriptorSet = 0;
    uint64_t BoneBufferID;
    std::vector<DirectX::XMMATRIX> BoneOffsets;
    std::vector<DirectX::XMMATRIX> BoneTransforms;
public:
    
    TempGameObject(std::vector<std::string> materials, std::string filename, std::string name, bool useSkinning = false);
    uint64_t GetBoneDescriptorSet() const { return BoneDescriptorSet; }
    uint64_t GetBoneBufferID() const { return BoneBufferID; }
    std::vector<DirectX::XMMATRIX> GetBoneOffsets() const { return BoneOffsets; }
    std::vector<DirectX::XMMATRIX> GetBoneTransforms() const { return BoneTransforms; }
    void AddSceneNode(const SceneNode& node);
};
