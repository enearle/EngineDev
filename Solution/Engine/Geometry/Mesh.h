#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN

#include <DirectXMath.h>
#include <string>
#include <vector>
#include "../RHI/RHI/RHIStructures.h"
#include "../ENGINE_API_Macro.h"
#include "../Scene/SceneNode.h"

struct aiScene;
struct DirectX::XMMATRIX;

class ENGINE_API Mesh
{
public:
    Mesh();
    Mesh(std::vector<RHIStructures::Vertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex);
    Mesh(std::vector<RHIStructures::SkinnedVertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex, 
        const std::vector<DirectX::XMMATRIX>& boneOffsets = {}, const std::vector<DirectX::XMMATRIX>& boneTransforms = {});
    ~Mesh();

    uint32_t GetVertexCount() const                     { return VertexCount; }
    uint32_t GetIndexCount() const                      { return IndexCount; }
    uint32_t GetLocalMaterialIndex() const              { return LocalMaterialIndex; }
    bool IsSkinned() const                              { return bIsSkinned; }
    uint32_t GetVertexStride() const                    { return bIsSkinned ? sizeof(RHIStructures::SkinnedVertex) : sizeof(RHIStructures::Vertex); }
    uint32_t GetPipelineVariantID() const               { return bIsSkinned ? 1 : 0; }
    uint64_t GetVertexBufferID() const                  { return VertexBufferID; }
    uint64_t GetIndexBufferID() const                   { return IndexBufferID; }
    void* GetVertexBufferHandle() const;
    void* GetIndexBufferHandle() const;
    std::vector<DirectX::XMMATRIX> GetBoneOffsets() const { return BoneOffsets; }
    std::vector<DirectX::XMMATRIX> GetBoneTransforms() const { return BoneTransforms; }
    
private:
    
    bool bIsSkinned = false;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t LocalMaterialIndex;
    
    uint64_t VertexBufferID = 0;
    uint64_t IndexBufferID = 0;
    std::vector<DirectX::XMMATRIX> BoneOffsets;
    std::vector<DirectX::XMMATRIX> BoneTransforms;
    
};

class ENGINE_API MeshNode : public SceneNode
{
public:
    MeshNode() = default;
    MeshNode(std::vector<Mesh>& meshes, const DirectX::XMMATRIX localMatrix, const std::string& name, SceneNode* parent = nullptr);
    ~MeshNode() = default;
    
    void AddMesh(Mesh mesh)                             { Meshes.push_back(std::move(mesh)); }
    
    std::vector<Mesh> GetMeshes() const                 { return Meshes; }
    size_t GetUVCount() const                           { return Meshes.size(); }
    const Mesh* GetMesh(uint32_t index) const;
    std::vector<Mesh> Meshes;
};
