#include "TempGameObject.h"
#include <iostream>
#include "RHI/Material.h"
#include "RHI/Geometry/GeometryImport.h"
#include "Renderer.h"
#include "Uploader.h"

void CollectBoneMatrices(const SceneNode& node, std::vector<DirectX::XMMATRIX>& outOffsetMatrices,
                         std::vector<DirectX::XMMATRIX>& outTransformMatrices)
{
    for (size_t i = 0; i < node.GetMeshCount(); i++)
    {
        const Mesh* mesh = node.GetMesh(i);
        if (mesh && mesh->IsSkinned())
        {
            const std::vector<DirectX::XMMATRIX>& boneOffsets = mesh->GetBoneOffsets();
            const std::vector<DirectX::XMMATRIX>& boneTransforms = mesh->GetBoneTransforms();
            
            if (boneOffsets.size() > outOffsetMatrices.size())
            {
                outOffsetMatrices.resize(boneOffsets.size());
                outTransformMatrices.resize(boneTransforms.size());
            }
            for (size_t j = 0; j < boneOffsets.size(); j++)
            {
                outOffsetMatrices[j] = boneOffsets[j];
                outTransformMatrices[j] = boneTransforms[j];
            }
        }
    }
    
    const auto& children = node.GetChildren();
    for (const auto& child : children)
    {
        CollectBoneMatrices(child, outOffsetMatrices, outTransformMatrices);
    }
}

TempGameObject::TempGameObject(std::vector<std::string> materials, std::string filename, std::string name, bool useSkinning)
{
    for (std::string material : materials)
    {
        Materials.push_back(Material(material, Material::PBR).LoadMaterial(1, 2));
    }
    
    MeshRoot = GeometryImport::CreateMeshGroup(filename, name, DirectX::XMMatrixIdentity(), useSkinning);
    ModelDataArray.reserve(100);
    
    if (useSkinning)
    {
        CollectBoneMatrices(MeshRoot.GetSceneNode(), BoneOffsets, BoneTransforms);
        
        std::vector<DirectX::XMFLOAT4X4> boneMatrices(MAX_BONES);
        for (uint32_t i = 0; i < MAX_BONES; i++)
        {
            DirectX::XMStoreFloat4x4(&boneMatrices[i], DirectX::XMMatrixIdentity());
        }
        
        size_t BONE_BUFFER_SIZE = MAX_BONES * sizeof(DirectX::XMFLOAT4X4);
        BoneBufferID = Uploader::UploadDynamic(BONE_BUFFER_SIZE, boneMatrices.data());
        BoneDescriptorSet = Uploader::AllocateDescriptor(BoneBufferID);
    }
    
    AddSceneNode(MeshRoot.GetSceneNode());
}

void TempGameObject::AddSceneNode(const SceneNode& node)
{
    for (size_t i = 0; i < node.GetMeshCount(); i++)
    {
        const Mesh* mesh = node.GetMesh(i);
        
        uint32_t materialIndex = mesh->GetLocalMaterialIndex();
        
        std::vector<uint64_t> descriptorSets = {Materials[materialIndex]};
        
        if (mesh->IsSkinned())
            descriptorSets.push_back(BoneDescriptorSet);

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
        indexedDraw.VertexStride = mesh->GetVertexStride();
        
        //Janky but works for now.
        IndexedDraw shadowDraw = indexedDraw;
        shadowDraw.PerDrawDescriptors.clear();  // Shadow pass doesn't need material textures
        if (mesh->IsSkinned())
            shadowDraw.PerDrawDescriptors.push_back(BoneDescriptorSet); 
        
        Renderer::AddIndexedDrawToContext(0, shadowDraw);
        Renderer::AddIndexedDrawToContext(1, indexedDraw);
    }

    std::vector<SceneNode> children = node.GetChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        AddSceneNode(children[i]);
    }
}
