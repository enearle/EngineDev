#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Game.h"
//
#include "EngineConstants.h"
#include "Camera.h"
#include "CameraController.h"
#include "DerpMover.h"
#include "Light.h"
#include "Input/InputEventSystem.h"
#include "NoesisUILayer.h"
#include "TempGameObject.h"
#include "GameTime.h"
#include "Uploader.h"
#include "../RHI/Renderer.h"
#include "Window.h"
#include "RHI/RHIConstants.h"

//using namespace EngineConstants;
//
//static Camera*           sCamera       = nullptr;
//static DerpMover*        sDerpMover    = nullptr;
//static CameraController* sCameraCon    = nullptr;
//static std::vector<TempGameObject*> sGameObjects; xc

void GameInit()
{
    //NoesisUILayer::Instance().NoesisInit();
//
    //sCamera = new Camera(90, 1280.0f / 720.0f, {0, 10, -8});
    //Camera::ActiveCamera = sCamera;
    //Camera::ActiveCamera->LookAtFloat3({});
//
    //LightData testLightData = {};
    //testLightData.Position  = DirectX::XMFLOAT3(-20.0f, 20.0f, -20.0f);
    //testLightData.Colour    = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    //testLightData.Intensity = 20.0f;
    //testLightData.Radius    = 50.0f;
    //testLightData.Type      = 1;
//
    //Light testLight = {testLightData, true};
//
    //std::vector<LightData> testLights = {testLightData, {}, {}, {}};
    //std::vector<DirectX::XMFLOAT4X4> lightMatrices = {
    //    testLight.GetLightMatrix(),
    //    DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4(), DirectX::XMFLOAT4X4()
    //};
//
    //Uploader::BufferID lightMatricesBufferID = Uploader::UploadDynamic(4 * sizeof(DirectX::XMFLOAT4X4), lightMatrices.data());
    //Uploader::BufferID lightDataBufferID     = Uploader::UploadDynamic(4 * sizeof(LightData), testLights.data());
//
    //std::vector pipelineDescs = { ShadowVSMPipeline(), PBRGeometryPipeline(), DeferredLightingPipeline() };
//
    //std::vector<std::vector<PipelineDescriptorData>> pipelineDescriptors = {
    //    {
    //        {1, {{0, lightMatricesBufferID, 0}}},
    //        {2, {{0, lightDataBufferID, 0}}}
    //    },
    //    { {1, {{0, Camera::GetBufferID(), 0}}} },
    //    {
    //        {1, {{0, Camera::GetBufferID(), 0}}},
    //        {4, {{0, lightDataBufferID, 0}}},
    //        {5, {{0, lightMatricesBufferID, 0}}}
    //    }
    //};
//
    //Renderer::CreatePipelines(pipelineDescs, pipelineDescriptors);
//
    //sGameObjects.push_back(new TempGameObject({"shells_0", "shells_1"}, "shells.fbx", "Shells"));
    //TempGameObject* derp = new TempGameObject({"derp_0"}, "Derp.fbx", "derp", true);
    //sGameObjects.push_back(derp);
//
    //sDerpMover = new DerpMover(derp);
    //sCameraCon = new CameraController(Camera::ActiveCamera, 5.0f);
//
    //Time::Init();
}

void GameRunFrame(float /*dt*/)
{
    //Time::UpdateTime();
    //InputEventSystem::PollInput(Renderer::GetWindow()->GetWindowHandle(), Time::GetDeltaTime());
    //InputEventSystem::ProcessCommands(Time::GetDeltaTime());
    //NoesisUILayer::Instance().NoesisUpdate(Time::GetDeltaTime());
    //if (sDerpMover)  sDerpMover->MoveTheDerp(Time::GetDeltaTime());
    //if (sCameraCon)  sCameraCon->MoveCamera(Time::GetDeltaTime());
    //Camera::UpdateMainCameraRenderData();
}

void GameShutdown()
{
    // ResetPipelines must run before deleting TempGameObjects to clear dangling
    // PushConstants pointers that reference TempGameObject::ModelDataArray memory
    //Renderer::ResetPipelines();
//
    //NoesisUILayer::Instance().NoesisShutdown();
//
    //delete sDerpMover;  sDerpMover = nullptr;
    //delete sCameraCon;  sCameraCon = nullptr;
//
    //Camera::ActiveCamera = nullptr;
    //delete sCamera;     sCamera = nullptr;
//
    //for (auto* obj : sGameObjects) delete obj;
    //sGameObjects.clear();
}
