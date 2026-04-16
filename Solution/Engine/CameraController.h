#pragma once
#include <DirectXMath.h>

class Camera;

class CameraController
{
    DirectX::XMFLOAT3 DirectionVector;
    Camera* TheCamera;
    float Speed;
public:
    CameraController(Camera* camera, float speed = 5.0f);
    void MoveCamera(double deltaTime);
    void AddInput( DirectX::XMFLOAT3 vectorToAdd);
};
