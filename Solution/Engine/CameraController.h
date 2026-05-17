#pragma once
#include <DirectXMath.h>
#include "ENGINE_API_Macro.h"
class Camera;

class ENGINE_API CameraController
{
    DirectX::XMFLOAT3 DirectionVector;
    Camera* TheCamera;
    float Speed;
public:
    CameraController(Camera* camera, float speed = 5.0f);
    void MoveCamera(double deltaTime);
    void AddInput( DirectX::XMFLOAT3 vectorToAdd);
};
