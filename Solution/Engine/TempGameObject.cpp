#include "TempGameObject.h"
#include "Common/RHI/Material.h"
#include "Common/RHI/Geometry/GeometryImport.h"
#include "Solution/RHI/Renderer.h"

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
        Materials.push_back(Material(material, Material::PBR).LoadMaterial(0, 1));
    }
    
    MeshRoot = GeometryImport::CreateMeshGroup(filename, name, DirectX::XMMatrixIdentity(), useSkinning);
    ModelDataArray.reserve(100);
    
    if (useSkinning)
    {
        constexpr uint32_t MAX_BONES = 128;
        
        CollectBoneMatrices(MeshRoot.GetSceneNode(), BoneOffsets, BoneTransforms);
    
        std::cout << "Collected " << BoneOffsets.size() << " bone offset matrices" << std::endl;
    
        std::vector<DirectX::XMFLOAT4X4> boneMatrices(MAX_BONES);
        for (uint32_t i = 0; i < MAX_BONES; i++)
        {
            DirectX::XMStoreFloat4x4(&boneMatrices[i], DirectX::XMMatrixIdentity());
        }
    
        BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
        constexpr size_t BONE_BUFFER_SIZE = MAX_BONES * sizeof(DirectX::XMFLOAT4X4);
        BufferDesc boneBufferDesc = RHIConstants::DefaultConstantBufferDesc;
        boneBufferDesc.Size = BONE_BUFFER_SIZE;
        boneBufferDesc.InitialData = boneMatrices.data();
        
        BoneBufferID = bufferAlloc->CreateBuffer(boneBufferDesc);
        std::cout << "Created bone buffer ID: " << BoneBufferID << ", size: " << BONE_BUFFER_SIZE << std::endl;
        std::vector<DescriptorSetBinding> bindings;
        bindings.emplace_back(DescriptorSetBinding {
            .Binding = 0,
            .ResourceID = BoneBufferID,
            .DynamicOffset = 0
        });
        
        BoneDescriptorSet = bufferAlloc->AllocateDescriptorSet(
            0,
            RHIConstants::VARIANT_DESCRIPTOR_SET_BASE,
            bindings
        );
        std::cout << "Created bone descriptor set ID: " << BoneDescriptorSet << std::endl;
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
        
        Renderer::AddIndexedDrawToContext(0, indexedDraw);
    }

    std::vector<SceneNode> children = node.GetChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        AddSceneNode(children[i]);
    }
}
