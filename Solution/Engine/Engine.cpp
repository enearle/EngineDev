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
#include "Uploader.h"
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
    
    Uploader::BufferID lightMatricesBufferID = Uploader::UploadDynamic(4 * sizeof(DirectX::XMFLOAT4X4), lightMatrices.data());
    Uploader::BufferID lightDataBufferID = Uploader::UploadDynamic(4 * sizeof(LightData), testLights.data());
    
    vector pipelineDescs = { ShadowVSMPipeline(), PBRGeometryPipeline(), DeferredLightingPipeline() };
    
    vector<vector<PipelineDescriptorData>> pipelineDescriptors = {
        {
            {1, {{0, lightMatricesBufferID, 0}}},
            {2, {{0, lightDataBufferID, 0}}} 
        },

        { {1, {{0, Camera::GetBufferID(), 0}}} },

        { 
            {1, {{0, Camera::GetBufferID(), 0}}},
            {4, {{0, lightDataBufferID, 0}}},
            {5, {{0, lightMatricesBufferID, 0}}}
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
