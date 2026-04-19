#include <chrono>
#include "EngineConstants.h"
#include "Camera.h"
#include "CameraController.h"
#include "InputEventSystem.h"
#include "NoesisUILayer.h"
#include "TempGameObject.h"
#include "Time.h"
#include "../RHI/Renderer.h"
#include "Common/Window.h"
#include "Common/RHI/RHIConstants.h"

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
    
    
    // Create pipelines
    vector pipelineDescs = { PBRGeometryPipeline(), DeferredLightingPipeline() };
    vector<vector<DescriptorSetBinding>> vpBindings = {{{0, Camera::GetBufferID(), 0}}, {{0, Camera::GetBufferID(), 0}}};
    
    Renderer::CreatePipelines(pipelineDescs, vpBindings);
    
    
    // Load a game object WIP (gameobject/staticmesh/scenegraph not complete)
    new TempGameObject({"shells_0", "shells_1"},"shells.fbx", "Shells");
    CameraController* CameraCon = new CameraController(Camera::ActiveCamera, 5.0f);
    
    Time::Init();
    
    while (!window->PeekMessages())
    {
        Time::UpdateTime();
        
        
        InputEventSystem::PollInput(window->GetWindowHandle(), Time::GetDeltaTime());
        InputEventSystem::ProcessCommands(Time::GetDeltaTime());
        
        
        NoesisUILayer::Instance().NoesisUpdate(Time::GetDeltaTime());
        
        
        //float radius = 15.0f;
        //float speed = 0.5f;
        //DirectX::XMFLOAT3 cameraPosition = {radius * sinf(Time::GetRunningTime() * speed), 10.0f, radius * cosf(Time::GetRunningTime() * speed)};
        //Camera::ActiveCamera->SetPositionFloat3(cameraPosition);
        //Camera::ActiveCamera->LookAtFloat3({});
        CameraCon->MoveCamera(Time::GetDeltaTime());
        Camera::UpdateMainCameraRenderData();
        
        
        Renderer::DrawFrame();
    }
    
    // Tear down
    Renderer::Wait();
    NoesisUILayer::Instance().NoesisShutdown();
    return Renderer::End();
}
