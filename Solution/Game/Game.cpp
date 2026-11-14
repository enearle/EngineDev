#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Game.h"
#include "EngineConstants.h"
#include "Camera.h"
#include "CameraController.h"
#include "DerpMover.h"
#include "Light.h"
#include "Input/InputEventSystem.h"
#include "UI/NoesisUILayer.h"
#include "TempGameObject.h"
#include "GameTime.h"
#include "Uploader.h"
#include "Renderer.h"
#include "Window.h"

using namespace EngineConstants;

static Camera*           sCamera       = nullptr;
static DerpMover*        sDerpMover    = nullptr;
static CameraController* sCameraCon    = nullptr;
static std::vector<TempGameObject*> sGameObjects;

void GameInit()
{

   NoesisUILayer::Instance().NoesisInit();
   sCamera = new Camera(90, 1280.0f / 720.0f, {0, 10, -8});
   Camera::ActiveCamera = sCamera;
   Camera::ActiveCamera->LookAtFloat3({});
   LightData testLightData = {};
   testLightData.Position  = DirectX::XMFLOAT3(-20.0f, 20.0f, -20.0f);
   testLightData.Colour    = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
   testLightData.Intensity = 20.0f;
   testLightData.Radius    = 50.0f;
   testLightData.Type      = 1;
   Light testLight = {testLightData, true};
   std::vector<LightData> testLights = {testLightData, {}, {}, {}};
   std::vector<DirectX::XMFLOAT4X4> lightMatrices = {
       testLight.GetLightMatrix(),
       DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4()
   };
   Uploader::BufferID lightMatricesBufferID = Uploader::UploadDynamic(4 * sizeof(DirectX::XMFLOAT4X4), lightMatrices.data());
   Uploader::BufferID lightDataBufferID     = Uploader::UploadDynamic(4 * sizeof(LightData), testLights.data());
   std::vector pipelineDescs = { ShadowVSMPipeline(), PBRGeometryPipeline(), DeferredLightingPipeline() };
   std::vector<std::vector<PipelineDescriptorData>> pipelineDescriptors = {
       {
           {1, {DescriptorSetBinding::ForBuffer(0, lightMatricesBufferID)}},
           {2, {DescriptorSetBinding::ForBuffer(0, lightDataBufferID)}}
       },
       { {1, {DescriptorSetBinding::ForBuffer(0, Camera::GetBufferID())}} },
       {
           {1, {DescriptorSetBinding::ForBuffer(0, Camera::GetBufferID())}},
           {4, {DescriptorSetBinding::ForBuffer(0, lightDataBufferID)}},
           {5, {DescriptorSetBinding::ForBuffer(0, lightMatricesBufferID)}}
       }
   };
   Renderer::CreatePipelines(pipelineDescs, pipelineDescriptors);
   sGameObjects.push_back(new TempGameObject({"shells_0", "shells_1"}, "../Engine/Meshes/shells.fbx", "Shells"));
   TempGameObject* derp = new TempGameObject({"derp_0"}, "../Engine/Meshes/Derp.fbx", "derp", true);
   sGameObjects.push_back(derp);
   sDerpMover = new DerpMover(derp);
   sCameraCon = new CameraController(Camera::ActiveCamera, 5.0f);
   GameTime::Init();
}

void GameRunFrame(float /*dt*/)
{
    GameTime::UpdateTime();
    InputEventSystem::PollInput(Renderer::GetWindow()->GetWindowHandle(), GameTime::GetDeltaTime());
    InputEventSystem::ProcessCommands(GameTime::GetDeltaTime());
    NoesisUILayer::Instance().NoesisUpdate(GameTime::GetDeltaTime());
    if (sDerpMover)  sDerpMover->MoveTheDerp(GameTime::GetDeltaTime());
    if (sCameraCon)  sCameraCon->MoveCamera(GameTime::GetDeltaTime());
    Camera::UpdateMainCameraRenderData();
}

void GameShutdown()
{
    Renderer::ResetPipelines();

    NoesisUILayer::Instance().NoesisShutdown();

    delete sDerpMover;  sDerpMover = nullptr;
    delete sCameraCon;  sCameraCon = nullptr;

    Camera::ActiveCamera = nullptr;
    delete sCamera;     sCamera = nullptr;

    for (auto* obj : sGameObjects) delete obj;
    sGameObjects.clear();
}
