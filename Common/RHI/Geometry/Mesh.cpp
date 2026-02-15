#include "Mesh.h"
#include "../BufferAllocator.h"
#include <stdexcept>

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
    
    MemoryAccess memoryAccess{0};
    memoryAccess.SetGPURead(true);
    memoryAccess.SetCPUWrite(true);
    
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    
    if (VertexCount > 0)
    {
        BufferDesc vertexBufferDesc = {};
        vertexBufferDesc.Size = vertices->size() * sizeof(Vertex);
        vertexBufferDesc.Usage = BufferUsage{
            .TransferSource = false,
            .TransferDestination = true,
            .Type = BufferType::Vertex
        };
        vertexBufferDesc.Type = BufferType::Vertex;
        vertexBufferDesc.Access = memoryAccess;
        vertexBufferDesc.InitialData = vertices->data();
        
        VertexBufferID = bufferAlloc->CreateBuffer(vertexBufferDesc, false);
    }
    
    if (IndexCount > 0)
    {
        BufferDesc indexBufferDesc = {};
        indexBufferDesc.Size = indices->size() * sizeof(uint32_t);
        indexBufferDesc.Usage = BufferUsage{
            .TransferSource = false,
            .TransferDestination = true,
            .Type = BufferType::Index
        };
        indexBufferDesc.Type = BufferType::Index;
        indexBufferDesc.Access = memoryAccess;
        indexBufferDesc.InitialData = indices->data();
        
        IndexBufferID = bufferAlloc->CreateBuffer(indexBufferDesc, false);
    }
}

Mesh::~Mesh()
{
    
}

const Mesh* SceneNode::GetMesh(uint32_t index) const
{
    {
        if (index >= Meshes.size())
            throw std::out_of_range("Mesh index: " + std::to_string(index) + " is out of range in mesh group: " + Name + ".");
        
        return &Meshes[index];
    }
}


void* Mesh::GetVertexBufferHandle() const
{
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    BufferAllocation allocation = bufferAlloc->GetBufferAllocation(VertexBufferID);
    
    // Return the platform-specific buffer handle
    return allocation.Buffer;  // VkBuffer or ID3D12Resource*
}

void* Mesh::GetIndexBufferHandle() const
{
    if (IndexBufferID == 0) return nullptr;
    
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    BufferAllocation allocation = bufferAlloc->GetBufferAllocation(IndexBufferID);
    
    return allocation.Buffer;
}
