#include <chrono>

#include "../RHI/Renderer.h"
#include "Common/Window.h"
#include "Common/RHI/Material.h"
#include "Common/RHI/RHIConstants.h"
#include "Common/RHI/Geometry/GeometryImport.h"


// A really bad way to handle this temporarily
std::vector<uint64_t> Materials;
RHIConstants::ModelData ModelData;
RHIConstants::VPData VPData;
uint64_t VPBufferID;
std::vector<RHIConstants::ModelData> ModelDataArray;
RootNode MeshRoot;
uint64_t geometryVPDescriptorSet;
uint64_t lightingVPDescriptorSet;

void AddSceneNode(const SceneNode& node)
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
        
        RHIConstants::ModelData mvpData;
        mvpData.ModelMatrix = model;
        mvpData.NormalMatrix = normal;
        ModelDataArray.push_back(mvpData);
        
        IndexedDraw indexedDraw;
        indexedDraw.VertexBufferID = mesh->GetVertexBufferID();
        indexedDraw.IndexBufferID = mesh->GetIndexBufferID();
        indexedDraw.VertexCount = mesh->GetVertexCount();
        indexedDraw.IndexCount = mesh->GetIndexCount();
        indexedDraw.PerDrawDescriptors = descriptorSets;
        indexedDraw.PushConstants = &ModelDataArray.back();
        indexedDraw.PushConstantSize = sizeof(RHIConstants::ModelData);
        
        Renderer::AddIndexedDrawToContext(0, indexedDraw);
    }

    std::vector<SceneNode> children = node.GetChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        AddSceneNode(children[i]);
    }
}

int main(int argc, char* argv[])
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);
    
    Renderer::Start(window);
    
    DirectX::XMFLOAT4 cameraPosition = {0.0, 10, -8, 1};
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&cameraPosition), 
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1));
    DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f);
    DirectX::XMMATRIX vp = view * projection;
    DirectX::XMStoreFloat4x4(&viewProj, vp);
    
    VPData.ViewProjection = viewProj;
    VPData.CameraPosition = cameraPosition;
    
    // Dynamic uniform
    BufferDesc VPBufferDesc = RHIConstants::DefaultConstantBufferDesc;
    VPBufferDesc.Size = 256;
    VPBufferDesc.Access = MemoryAccess(9);
    VPBufferDesc.InitialData = &VPData;
    
    VPBufferID = BufferAllocator::GetInstance()->CreateBuffer(VPBufferDesc);
    BufferAllocation vpAllocation = BufferAllocator::GetInstance()->GetBufferAllocation(VPBufferID);
    if (!vpAllocation.IsMapped || vpAllocation.Address == nullptr)
        throw std::runtime_error("VP buffer is not mapped! Check MemoryAccess flags.");
    
    // Create pipelines
    std::vector<class Pipeline*> pipelines;
    pipelines.push_back(RHIConstants::PBRGeometryPipeline());
    std::vector<IOResource> inputResources = {*pipelines[0]->GetOutputResource()};
    for (auto& binding : inputResources[0].Layout.Bindings)
    {
        binding.Set = 1;  // Temporary, need a better way to manage IODescriptors
    }
    pipelines.push_back(RHIConstants::DeferredLightingPipeline(&inputResources));
    
    // Create texture images
    Materials.push_back(Material("shells_0", Material::PBR).LoadMaterial(0,1));
    Materials.push_back(Material("shells_1", Material::PBR).LoadMaterial(0,1));
    
    std::vector<DescriptorSetBinding> vpBindings = {
        {0, VPBufferID, 0}
    };
    geometryVPDescriptorSet = BufferAllocator::GetInstance()->AllocateDescriptorSet(0, 0, vpBindings);
    lightingVPDescriptorSet = BufferAllocator::GetInstance()->AllocateDescriptorSet(1, 0, vpBindings);
    
    // Create vertex/index buffers
    MeshRoot = GeometryImport::CreateMeshGroup("shells.fbx", "Shells", DirectX::XMMatrixIdentity());
    ModelDataArray.reserve(100);
    
    // Submit to renderer
    Renderer::CreatePipelineFrameContext(pipelines[0], false, false);
    Renderer::CreatePipelineFrameContext(pipelines[1], true, true);
    Renderer::AddDescriptorIDToContext(0, geometryVPDescriptorSet);
    Renderer::AddDescriptorIDToContext(1, lightingVPDescriptorSet);
    AddSceneNode(MeshRoot.GetSceneNode());
    std::cout << "Engine initialized with " << ModelDataArray.size() << " MVP data entries." << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (!window->PeekMessages())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        float radius = 15.0f;
        float speed = 0.5f;
        
        DirectX::XMFLOAT4 cameraPosition = {
            radius * sinf(time * speed),
            10.0f,
            radius * cosf(time * speed),
            1.0f
        };
        
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat4(&cameraPosition), 
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        );
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XM_PIDIV2, 1280.0f / 720.0f, 0.1f, 100.0f
        );
        DirectX::XMMATRIX vp = view * projection;
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMStoreFloat4x4(&viewProj, vp);
        
        RHIConstants::VPData* mappedVP = static_cast<RHIConstants::VPData*>(vpAllocation.Address);
        mappedVP->ViewProjection = viewProj;
        mappedVP->CameraPosition = cameraPosition;
        
        Renderer::DrawFrame();
    }
    
    return Renderer::End();
}
