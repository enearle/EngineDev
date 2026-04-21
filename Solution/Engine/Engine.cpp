#include <chrono>
#include "EngineConstants.h"
#include "Camera.h"
#include "CameraController.h"
#include "DerpMover.h"
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
    
    
    // Create pipelines
    vector pipelineDescs = { PBRGeometryPipeline(), DeferredLightingPipeline() };
    vector<vector<DescriptorSetBinding>> vpBindings = {{{0, Camera::GetBufferID(), 0}}, {{0, Camera::GetBufferID(), 0}}};
    
    Renderer::CreatePipelines(pipelineDescs, vpBindings);
    
    
    // Load a game object WIP (gameobject/staticmesh/scenegraph not complete)
    new TempGameObject({"shells_0", "shells_1"},"shells.fbx", "Shells");
    TempGameObject* derp = new TempGameObject({"derp_0"}, "Derp.fbx", "derp", true);
    
    // These aren't real objects they ar just a temporary placeholder
    DerpMover* derpMover = new DerpMover(derp);
    CameraController* cameraCon = new CameraController(Camera::ActiveCamera, 5.0f);
    
    Time::Init();
    
    while (!window->PeekMessages())
    {
        Time::UpdateTime();
        
        InputEventSystem::PollInput(window->GetWindowHandle(), Time::GetDeltaTime());
        InputEventSystem::ProcessCommands(Time::GetDeltaTime());
        
        NoesisUILayer::Instance().NoesisUpdate(Time::GetDeltaTime());
        
        uint64_t deprBufferId = derp->GetBoneDescriptorSet();
        
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
