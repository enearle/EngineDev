#pragma once
#include "EngineConstants.h"
#include "ENGINE_API_Macro.h"

class SceneNode;

class ENGINE_API TempGameObject
{
    std::vector<RHIStructures::DescriptorSetHandle> Materials;
    SceneNode* SceneRoot;
    RHIStructures::DescriptorSetHandle BoneDescriptorSet = {};
    RHIStructures::BufferHandle BoneBufferID = {};
    std::vector<DirectX::XMMATRIX> BoneOffsets;
    std::vector<DirectX::XMMATRIX> BoneTransforms;
    const uint32_t MAX_BONES = 128;
public:

    TempGameObject(std::vector<std::string> materials, std::string filename, std::string name, bool useSkinning = false);
    RHIStructures::DescriptorSetHandle GetBoneDescriptorSet() const { return BoneDescriptorSet; }
    RHIStructures::BufferHandle GetBoneBufferID() const { return BoneBufferID; }
    const std::vector<DirectX::XMMATRIX>& GetBoneOffsets() const { return BoneOffsets; }
    const std::vector<DirectX::XMMATRIX>& GetBoneTransforms() const { return BoneTransforms; }
    void CreateDrawForMeshNode(SceneNode* node);
    void UploadToGPU();
    void FreeGPUResources();
};
