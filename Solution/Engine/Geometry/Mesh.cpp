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
    
    if (VertexCount > 0)
        VertexBufferID = Uploader::UploadVertices(vertexCache->Vertices.size() * sizeof(Vertex), vertexCache->Vertices.data());
    
    if (IndexCount > 0)
        IndexBufferID = Uploader::UploadIndices(vertexCache->Indices.size() * sizeof(uint32_t), vertexCache->Indices.data());
}

Mesh::Mesh(SkinnedVertexCache* skinnedVertexCache, uint32_t LocalMaterialIndex, 
    const  std::vector<DirectX::XMMATRIX>& boneOffsets, const std::vector<DirectX::XMMATRIX>& boneTransforms) :
    LocalMaterialIndex(LocalMaterialIndex), bIsSkinned(true), BoneOffsets(boneOffsets), BoneTransforms(boneTransforms)
{
    using namespace RHIStructures;
    
    VertexCount = skinnedVertexCache->Vertices.size();
    IndexCount = skinnedVertexCache->Indices.size();
    TempSkinnedVertexCache = skinnedVertexCache;
    
    if (VertexCount > 0)
        VertexBufferID = Uploader::UploadVertices(skinnedVertexCache->Vertices.size() * sizeof(SkinnedVertex), skinnedVertexCache->Vertices.data());
    
    if (IndexCount > 0)
        IndexBufferID = Uploader::UploadIndices(skinnedVertexCache->Indices.size() * sizeof(uint32_t), skinnedVertexCache->Indices.data());
}

Mesh::~Mesh()
{
    WipeCachedData();
}

const Mesh* MeshNode::GetMesh(uint32_t index) const
{
    if (index >= Meshes.size())
        throw std::out_of_range("Mesh index: " + std::to_string(index) + " is out of range in mesh group: " + Name + ".");
        
    return &Meshes[index];
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

void* Mesh::GetCachedIndexData() const
{
    if (bIsSkinned && TempSkinnedVertexCache)
            return TempSkinnedVertexCache->Indices.data();
    
    else if (TempVertexCache)
            return TempVertexCache->Indices.data();
    
    throw std::runtime_error("No cached index data available.");
}

void* Mesh::GetCachedVertexData() const
{
    if (bIsSkinned && TempSkinnedVertexCache)
            return TempSkinnedVertexCache->Vertices.data();
    
    else if (TempVertexCache)
            return TempVertexCache->Vertices.data();
    
    throw std::runtime_error("No cached vertex data available.");
}

void Mesh::WipeCachedData()
{
    delete TempSkinnedVertexCache;
    delete TempVertexCache;
}

MeshNode::MeshNode(std::vector<Mesh>& meshes, const DirectX::XMMATRIX localMatrix, const std::string& name, SceneNode* parent) : Meshes(std::move(meshes))
{
    SceneNode::Init(name, parent, localMatrix);
}
