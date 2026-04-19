#include "TempGameObject.h"
#include "Common/RHI/Material.h"
#include "Common/RHI/Geometry/GeometryImport.h"
#include "Solution/RHI/Renderer.h"

TempGameObject::TempGameObject(std::vector<std::string> materials, std::string filename, std::string name)
{
    for (std::string material : materials)
    {
        Materials.push_back(Material(material, Material::PBR).LoadMaterial(0, 1));
    }
    
    MeshRoot = GeometryImport::CreateMeshGroup(filename, name, DirectX::XMMatrixIdentity());
    ModelDataArray.reserve(100);
    
    AddSceneNode(MeshRoot.GetSceneNode());
}

void TempGameObject::AddSceneNode(const SceneNode& node)
{
    for (size_t i = 0; i < node.GetMeshCount(); i++)
    {
        const Mesh* mesh = node.GetMesh(i);
        
        uint32_t materialIndex = mesh->GetLocalMaterialIndex();
        std::vector<uint64_t> descriptorSets = {Materials[materialIndex]};

        DirectX::XMMATRIX modelMatrix = node.GetModelMatrix();
        DirectX::XMFLOAT4X4 model;
        DirectX::XMStoreFloat4x4(&model, modelMatrix);

        DirectX::XMFLOAT4X4 normal;
        DirectX::XMMATRIX inverseTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, modelMatrix));
        DirectX::XMStoreFloat4x4(&normal, inverseTranspose);
        
        EngineConstants::ModelData mvpData;
        mvpData.ModelMatrix = model;
        mvpData.NormalMatrix = normal;
        ModelDataArray.push_back(mvpData);
        
        IndexedDraw indexedDraw;
        indexedDraw.PipelineVarientID = mesh->GetPipelineVariantID();
        indexedDraw.VertexBufferID = mesh->GetVertexBufferID();
        indexedDraw.IndexBufferID = mesh->GetIndexBufferID();
        indexedDraw.VertexCount = mesh->GetVertexCount();
        indexedDraw.IndexCount = mesh->GetIndexCount();
        indexedDraw.PerDrawDescriptors = descriptorSets;
        indexedDraw.PushConstants = &ModelDataArray.back();
        indexedDraw.PushConstantSize = sizeof(EngineConstants::ModelData);
        
        Renderer::AddIndexedDrawToContext(0, indexedDraw);
    }

    std::vector<SceneNode> children = node.GetChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        AddSceneNode(children[i]);
    }
}
