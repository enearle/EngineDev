#include "Mesh.h"

#include <iostream>

#include "../RHI/Uploader.h"
#include <stdexcept>
#include "RHI/RHIConstants.h"

using namespace RHIStructures;

Mesh::Mesh()
{
    VertexCount = 0;
    IndexCount = 0;
    LocalMaterialIndex = 0;
}

Mesh::Mesh(std::vector<Vertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex) :
    LocalMaterialIndex(LocalMaterialIndex)
{
    using namespace RHIStructures;
    
    VertexCount = vertices->size();
    IndexCount = indices->size();
    
    if (VertexCount > 0)
        VertexBufferID = Uploader::UploadVertices(vertices->size() * sizeof(Vertex), vertices->data());
    
    if (IndexCount > 0)
        IndexBufferID = Uploader::UploadIndices(indices->size() * sizeof(uint32_t), indices->data());
}

Mesh::Mesh(std::vector<SkinnedVertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex, 
    const  std::vector<DirectX::XMMATRIX>& boneOffsets, const std::vector<DirectX::XMMATRIX>& boneTransforms) :
    LocalMaterialIndex(LocalMaterialIndex), bIsSkinned(true), BoneOffsets(boneOffsets), BoneTransforms(boneTransforms)
{
    using namespace RHIStructures;
    
    VertexCount = vertices->size();
    IndexCount = indices->size();
    
    if (VertexCount > 0)
        VertexBufferID = Uploader::UploadVertices(vertices->size() * sizeof(SkinnedVertex), vertices->data());
    
    if (IndexCount > 0)
        IndexBufferID = Uploader::UploadIndices(indices->size() * sizeof(uint32_t), indices->data());
}

Mesh::~Mesh()
{
    
}

const Mesh* MeshNode::GetMesh(uint32_t index) const
{
    {
        if (index >= Meshes.size())
            throw std::out_of_range("Mesh index: " + std::to_string(index) + " is out of range in mesh group: " + Name + ".");
        
        return &Meshes[index];
    }
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

MeshNode::MeshNode(std::vector<Mesh>& meshes, const DirectX::XMMATRIX localMatrix, const std::string& name, SceneNode* parent) : Meshes(std::move(meshes))
{
    SceneNode::Init(name, parent, localMatrix);
}
