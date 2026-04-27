#include <chrono>
#include "EngineConstants.h"
#include "Camera.h"
#include "CameraController.h"
#include "DerpMover.h"
#include "Light.h"
#include "Input/InputEventSystem.h"
#include "NoesisUILayer.h"
#include "TempGameObject.h"
#include "Time.h"
#include "../RHI/Renderer.h"
#include "Window.h"
#include "RHI/RHIConstants.h"

using namespace EngineConstants;
using namespace std;

int main(int argc, char* argv[])
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);
    Renderer::Start(window);
    
    
    // Noesis Initialization
    NoesisUILayer::Instance().NoesisInit();
    
    new Camera(90, 1280.0f / 720.0f, {0,10,-8});
    Camera::ActiveCamera->LookAtFloat3({});
    
    // Test light buffer
    LightData testLightData = {};
    testLightData.Position = DirectX::XMFLOAT3(-20.0, 20.0, -20.0);
    testLightData.Colour = DirectX::XMFLOAT3(1.0, 1.0, 1.0);
    testLightData.Intensity = 20.0;
    testLightData.Radius = 50.0;
    testLightData.Type = 1;
    
    Light testLight = {testLightData, true};
    
    std::vector<LightData> testLights = {testLightData, {}, {}, {}};
    std::vector<DirectX::XMFLOAT4X4> lightMatrices = {testLight.GetLightMatrix(), DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4()};
    
    BufferDesc lightMatricesBuffer = RHIConstants::DefaultConstantBufferDesc;
    lightMatricesBuffer.Size = 4 * sizeof(DirectX::XMFLOAT4X4);
    lightMatricesBuffer.Access = MemoryAccess(9);
    lightMatricesBuffer.InitialData = lightMatrices.data();
    uint64_t lightMatricesBufferID = BufferAllocator::GetInstance()->CreateBuffer(lightMatricesBuffer);
    BufferAllocation lightMatAllocation = BufferAllocator::GetInstance()->GetBufferAllocation(lightMatricesBufferID);
    
    BufferDesc lightDataBuffer = RHIConstants::DefaultConstantBufferDesc;
    lightDataBuffer.Size = 4 * sizeof(LightData);
    lightDataBuffer.Access = MemoryAccess(9);
    lightDataBuffer.InitialData = testLights.data();
    uint64_t lightDataBufferID = BufferAllocator::GetInstance()->CreateBuffer(lightDataBuffer);
    BufferAllocation lightDataAllocation = BufferAllocator::GetInstance()->GetBufferAllocation(lightDataBufferID);
    
    vector pipelineDescs = { ShadowVSMPipeline(), PBRGeometryPipeline(), DeferredLightingPipeline() };
    
    vector<vector<PipelineDescriptorData>> pipelineDescriptors = {
        // Pipeline 0: Shadow - only Set 0
        {
            {0, {{0, lightMatricesBufferID, 0}}},
            {1, {{0, lightDataBufferID, 0}}} 
        },
    
        // Pipeline 1: PBR - only Set 0
        { {0, {{0, Camera::GetBufferID(), 0}}} },
    
        // Pipeline 2: Deferred Lighting - Set 0 AND Set 3
        { 
            {0, {{0, Camera::GetBufferID(), 0}}},
            {3, {{0, lightDataBufferID, 0}}},
            {4, {{0, lightMatricesBufferID, 0}}}
        }
    };
    
    Renderer::CreatePipelines(pipelineDescs, pipelineDescriptors);
    
    // Load a game object WIP (gameobject/staticmesh/scenegraph not complete)
    new TempGameObject({"shells_0", "shells_1"},"shells.fbx", "Shells");
    TempGameObject* derp = new TempGameObject({"derp_0"}, "Derp.fbx", "derp", true);
    
    // These aren't real objects they are just a temporary placeholder
    DerpMover* derpMover = new DerpMover(derp);
    CameraController* cameraCon = new CameraController(Camera::ActiveCamera, 5.0f);
    
    Time::Init();
    
    while (!window->PeekMessages())
    {
        Time::UpdateTime();
        
        InputEventSystem::PollInput(window->GetWindowHandle(), Time::GetDeltaTime());
        InputEventSystem::ProcessCommands(Time::GetDeltaTime());
        
        NoesisUILayer::Instance().NoesisUpdate(Time::GetDeltaTime());
        
        derpMover->MoveTheDerp(Time::GetDeltaTime());
        cameraCon->MoveCamera(Time::GetDeltaTime());
        Camera::UpdateMainCameraRenderData();
        
        Renderer::DrawFrame();
    }
    
    // Tear down
    Renderer::Wait();
    NoesisUILayer::Instance().NoesisShutdown();
    return Renderer::End();
}
