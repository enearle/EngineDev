#include "TempGameObject.h"
#include <iostream>
#include "RHI/Material.h"
#include "Geometry/GeometryImport.h"
#include "Renderer.h"
#include "Uploader.h"
#include "Geometry/Mesh.h"
#include "Scene/SceneNode.h"

void CollectBoneMatrices(const SceneNode* node, std::vector<DirectX::XMMATRIX>& outOffsetMatrices,
                         std::vector<DirectX::XMMATRIX>& outTransformMatrices)
{
    MeshAsset* meshAsset = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType))->GetMeshAsset();
    
    for (size_t i = 0; i < meshAsset->GetMeshCount(); i++)
    {
        const Mesh* mesh = &meshAsset->GetMesh(i);
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
    
    for (SceneNode* child : node->GetChildren())
    {
        SceneNode* sceneNode = child;
        if (sceneNode)
            CollectBoneMatrices(sceneNode, outOffsetMatrices, outTransformMatrices);
    }
}

TempGameObject::TempGameObject(std::vector<std::string> materials, std::string filename, std::string name, bool useSkinning)
{
    for (std::string material : materials)
    {
        Materials.push_back(Material(material, Material::PBR).LoadMaterial(1, 2));
    }
    
    SceneRoot = GeometryImport::CreateMeshGroup(filename, name, DirectX::XMMatrixIdentity(), useSkinning);
    ModelDataArray.reserve(100);
    
    if (useSkinning)
    {
        CollectBoneMatrices(SceneRoot, BoneOffsets, BoneTransforms);
        
        std::vector<DirectX::XMFLOAT4X4> boneMatrices(MAX_BONES);
        for (uint32_t i = 0; i < MAX_BONES; i++)
        {
            DirectX::XMStoreFloat4x4(&boneMatrices[i], DirectX::XMMatrixIdentity());
        }
        
        size_t BONE_BUFFER_SIZE = MAX_BONES * sizeof(DirectX::XMFLOAT4X4);
        BoneBufferID = Uploader::UploadDynamic(BONE_BUFFER_SIZE, boneMatrices.data());
        BoneDescriptorSet = Uploader::AllocateDescriptor(BoneBufferID);
    }
    
    CreateDrawForMeshNode(SceneRoot);
}

void TempGameObject::CreateDrawForMeshNode(SceneNode* node)
{
    MeshAsset* meshAsset = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType))->GetMeshAsset();
    
    for (size_t i = 0; i < meshAsset->GetMeshCount(); i++)
    {
        const Mesh* mesh = &meshAsset->GetMesh(i);
        
        uint32_t materialIndex = mesh->GetLocalMaterialIndex();
        
        std::vector<uint64_t> descriptorSets = {Materials[materialIndex]};
        
        if (mesh->IsSkinned())
            descriptorSets.push_back(BoneDescriptorSet);

        DirectX::XMMATRIX modelMatrix = node->GetLocalMatrix();
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

    for (SceneNode* child : node->GetChildren())
    {
        SceneNode* meshNode = child;
        if (meshNode)
            CreateDrawForMeshNode(meshNode);
    }
}

void TempGameObject::UploadToGPU()
{
    SceneRoot->UploadToGPU();
}

void TempGameObject::FreeGPUResources()
{
    SceneRoot->FreeGPUResources();
}
