#include "CameraController.h"
#include <windows.h>
#include "Camera.h"
#include "InputEventSystem.h"
#include "DirectXMath.h"

CameraController::CameraController(Camera* camera, float speed)
{
    InputEventSystem::RegisterCommand(InputMode::Gameplay, "W", KeyAction::Held, [this](double) { AddInput({0, 0, 1}); });
    InputEventSystem::RegisterCommand(InputMode::Gameplay, "S", KeyAction::Held, [this](double) { AddInput({0, 0, -1}); });
    InputEventSystem::RegisterCommand(InputMode::Gameplay, "A", KeyAction::Held, [this](double) { AddInput({-1, 0, 0}); });
    InputEventSystem::RegisterCommand(InputMode::Gameplay, "D", KeyAction::Held, [this](double) { AddInput({1, 0, 0}); });
    InputEventSystem::RegisterCommand(InputMode::Gameplay, std::string(1, VK_SPACE), KeyAction::Held, [this](double) { AddInput({0, 1, 0}); });
    InputEventSystem::RegisterCommand(InputMode::Gameplay, std::string(1, VK_SHIFT), KeyAction::Held, [this](double) { AddInput({0, -1, 0}); });
    TheCamera = camera;
    Speed = speed;
}

void CameraController::MoveCamera(double deltaTime)
{
    DirectX::XMVECTOR moveDirection = DirectX::XMLoadFloat3(&DirectionVector);
    moveDirection = DirectX::XMVector3Normalize(moveDirection);
    moveDirection = DirectX::XMVectorScale(moveDirection, Speed * deltaTime);
    TheCamera->SetPositionVector(DirectX::XMVectorAdd(moveDirection, TheCamera->GetPositionVector()));
    DirectionVector = {0, 0, 0};
}

void CameraController::AddInput(DirectX::XMFLOAT3 vectorToAdd)
{
    DirectionVector.x += vectorToAdd.x;
    DirectionVector.y += vectorToAdd.y;
    DirectionVector.z += vectorToAdd.z;
}
