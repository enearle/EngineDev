#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN

#include <DirectXMath.h>
#include <string>
#include <vector>
#include "../RHI/RHI/RHIStructures.h"
#include "../ENGINE_API_Macro.h"
#include "../RHI/Uploader.h"

struct aiScene;
struct DirectX::XMMATRIX;

struct SkinnedVertexCache
{
    std::vector<RHIStructures::SkinnedVertex> Vertices;
    std::vector<uint32_t> Indices;
};

struct VertexCache
{
    std::vector<RHIStructures::Vertex> Vertices;
    std::vector<uint32_t> Indices;
};

class ENGINE_API Mesh
{
public:
    Mesh();
    Mesh(VertexCache* vertexCache, uint32_t LocalMaterialIndex);
    Mesh(SkinnedVertexCache* skinnedVertexCache, uint32_t LocalMaterialIndex,
        const std::vector<DirectX::XMMATRIX>& boneOffsets = {}, const std::vector<DirectX::XMMATRIX>& boneTransforms = {});
    ~Mesh();

    // Move-only: cache pointers are owned exclusively; copy would double-free.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    uint32_t GetVertexCount() const                     { return VertexCount; }
    uint32_t GetIndexCount() const                      { return IndexCount; }
    uint32_t GetLocalMaterialIndex() const              { return LocalMaterialIndex; }
    bool IsSkinned() const                              { return bIsSkinned; }
    uint32_t GetVertexStride() const                    { return bIsSkinned ? sizeof(RHIStructures::SkinnedVertex) : sizeof(RHIStructures::Vertex); }
    uint32_t GetPipelineVariantID() const               { return bIsSkinned ? 1 : 0; }
    Uploader::BufferID GetVertexBufferID() const        { return VertexBufferID; }
    Uploader::BufferID GetIndexBufferID() const         { return IndexBufferID; }
    void* GetVertexBufferHandle() const;
    void* GetIndexBufferHandle() const;
    std::vector<DirectX::XMMATRIX> GetBoneOffsets() const { return BoneOffsets; }
    std::vector<DirectX::XMMATRIX> GetBoneTransforms() const { return BoneTransforms; }
    void UploadToGPU();
private:
    
    bool bIsSkinned = false;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t LocalMaterialIndex;
    std::vector<DirectX::XMMATRIX> BoneOffsets;
    std::vector<DirectX::XMMATRIX> BoneTransforms;
    
    Uploader::BufferID VertexBufferID = 0;
    Uploader::BufferID IndexBufferID = 0;
    VertexCache* TempVertexCache = nullptr;
    SkinnedVertexCache* TempSkinnedVertexCache = nullptr;
    
public:
    void* GetCachedIndexData() const;
    void* GetCachedVertexData() const;
    void WipeCachedData();
    
};