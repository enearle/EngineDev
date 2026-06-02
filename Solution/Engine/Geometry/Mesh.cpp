#include "Mesh.h"
#include <stdexcept>
#include "RHI/RHIConstants.h"

using namespace RHIStructures;

Mesh::Mesh()
{
    VertexCount = 0;
    IndexCount = 0;
    LocalMaterialIndex = 0;
}

Mesh::Mesh(VertexCache* vertexCache, uint32_t LocalMaterialIndex) :
    LocalMaterialIndex(LocalMaterialIndex)
{
    using namespace RHIStructures;
    
    VertexCount = vertexCache->Vertices.size();
    IndexCount = vertexCache->Indices.size();
    TempVertexCache = vertexCache;
}

Mesh::Mesh(SkinnedVertexCache* skinnedVertexCache, uint32_t LocalMaterialIndex, 
    const  std::vector<DirectX::XMMATRIX>& boneOffsets, const std::vector<DirectX::XMMATRIX>& boneTransforms) :
    LocalMaterialIndex(LocalMaterialIndex), bIsSkinned(true), BoneOffsets(boneOffsets), BoneTransforms(boneTransforms)
{
    using namespace RHIStructures;
    
    VertexCount = skinnedVertexCache->Vertices.size();
    IndexCount = skinnedVertexCache->Indices.size();
    TempSkinnedVertexCache = skinnedVertexCache;
}

Mesh::~Mesh()
{
    WipeCachedData();
}

Mesh::Mesh(Mesh&& other) noexcept
    : bIsSkinned(other.bIsSkinned),
      VertexCount(other.VertexCount),
      IndexCount(other.IndexCount),
      LocalMaterialIndex(other.LocalMaterialIndex),
      BoneOffsets(std::move(other.BoneOffsets)),
      BoneTransforms(std::move(other.BoneTransforms)),
      VertexBufferID(other.VertexBufferID),
      IndexBufferID(other.IndexBufferID),
      TempVertexCache(other.TempVertexCache),
      TempSkinnedVertexCache(other.TempSkinnedVertexCache)
{
    other.TempVertexCache = nullptr;
    other.TempSkinnedVertexCache = nullptr;
    other.VertexBufferID = 0;
    other.IndexBufferID = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        WipeCachedData();
        bIsSkinned = other.bIsSkinned;
        VertexCount = other.VertexCount;
        IndexCount = other.IndexCount;
        LocalMaterialIndex = other.LocalMaterialIndex;
        BoneOffsets = std::move(other.BoneOffsets);
        BoneTransforms = std::move(other.BoneTransforms);
        VertexBufferID = other.VertexBufferID;
        IndexBufferID = other.IndexBufferID;
        TempVertexCache = other.TempVertexCache;
        TempSkinnedVertexCache = other.TempSkinnedVertexCache;

        other.TempVertexCache = nullptr;
        other.TempSkinnedVertexCache = nullptr;
        other.VertexBufferID = 0;
        other.IndexBufferID = 0;
    }
    return *this;
}

void* Mesh::GetVertexBufferHandle() const
{
    BufferAllocation allocation = Uploader::GetBufferAllocation(VertexBufferID);
    
    // Return the platform-specific buffer handle
    return allocation.Buffer;  // VkBuffer or ID3D12Resource*
}

void* Mesh::GetIndexBufferHandle() const
{
    if (IndexBufferID == 0) return nullptr;
    
    BufferAllocation allocation = Uploader::GetBufferAllocation(IndexBufferID);
    
    return allocation.Buffer;
}

void Mesh::UploadToGPU()
{
    if (!bIsSkinned && TempVertexCache)
    {
        if (VertexCount > 0)
            VertexBufferID = Uploader::UploadVertices(TempVertexCache->Vertices.size() * sizeof(Vertex), TempVertexCache->Vertices.data());
    
        if (IndexCount > 0)
            IndexBufferID = Uploader::UploadIndices(TempVertexCache->Indices.size() * sizeof(uint32_t), TempVertexCache->Indices.data());
    }
    else if (bIsSkinned && TempSkinnedVertexCache)
    {
        if (VertexCount > 0)
            VertexBufferID = Uploader::UploadVertices(TempSkinnedVertexCache->Vertices.size() * sizeof(Vertex), TempSkinnedVertexCache->Vertices.data());
        
        if (IndexCount > 0)
            IndexBufferID = Uploader::UploadIndices(TempSkinnedVertexCache->Indices.size() * sizeof(uint32_t), TempSkinnedVertexCache->Indices.data());
    }
}

void* Mesh::GetCachedIndexData() const
{
    if (!bIsSkinned &&TempVertexCache)
        return TempVertexCache->Indices.data();
    
    if (bIsSkinned && TempSkinnedVertexCache)
        return TempSkinnedVertexCache->Indices.data();

    throw std::runtime_error("No cached index data available.");
}

void* Mesh::GetCachedVertexData() const
{
    if (!bIsSkinned && TempVertexCache)
        return TempVertexCache->Vertices.data();
    
    if (bIsSkinned && TempSkinnedVertexCache)
        return TempSkinnedVertexCache->Vertices.data();
    
    throw std::runtime_error("No cached vertex data available.");
}

void Mesh::WipeCachedData()
{
    delete TempSkinnedVertexCache; TempSkinnedVertexCache = nullptr;
    delete TempVertexCache;        TempVertexCache        = nullptr;
}
