#include "Mesh.h"

#include <stdexcept>


Mesh::Mesh()
{
    VertexCount = 0;
    IndexCount = 0;
    LocalMaterialIndex = 0;
}

Mesh::Mesh(std::vector<RHIStructures::Vertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex) :
    LocalMaterialIndex(LocalMaterialIndex)
{
    VertexCount = vertices->size();
    IndexCount = indices->size();
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


