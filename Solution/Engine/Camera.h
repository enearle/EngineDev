#pragma once
#include <DirectXMath.h>
#include "ENGINE_API_Macro.h"
#include "Renderer.h"

class ENGINE_API Camera
{
    float FOV;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
    DirectX::XMMATRIX Transform;
    DirectX::XMMATRIX View;
    DirectX::XMMATRIX Projection;
    size_t ResizeSubscription = 0;

    void SubscribeToResize()
    {
        ResizeSubscription = Renderer::EventOnResize().Subscribe([this](uint32_t width, uint32_t height) { OnResize(width, height); });
    }

public:
    static Camera* ActiveCamera;

    Camera(float fovDeg, float aspectRatio, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation = {}, float nearPlane = 0.1f, float farPlane = 100.0f)
        : FOV(fovDeg / 180.0f * DirectX::XM_PI), AspectRatio(aspectRatio), NearPlane(nearPlane), FarPlane(farPlane)
    {
        SetTransform(DirectX::XMMatrixTranslation(position.x, position.y, position.z) * DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z));
        Projection = DirectX::XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
        SubscribeToResize();
    }

    Camera(float fovDeg, float aspectRatio, const DirectX::XMMATRIX& transformOrView, bool isView = false, float nearPlane = 0.1f, float farPlane = 1000.0f)
        : FOV(fovDeg / 180.0f * DirectX::XM_PI), AspectRatio(aspectRatio), NearPlane(nearPlane), FarPlane(farPlane)
    {
        isView ? SetView(transformOrView) : SetTransform(transformOrView);
        Projection = DirectX::XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
        SubscribeToResize();
    }

    ~Camera()
    {
        Renderer::EventOnResize().Unsubscribe(ResizeSubscription);
        if (ActiveCamera == this)
            ActiveCamera = nullptr;
    }

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    void SetView(DirectX::XMMATRIX view) { View = view; Transform = DirectX::XMMatrixInverse(nullptr, view); }
    void SetTransform(DirectX::XMMATRIX transform) { Transform = transform; View = DirectX::XMMatrixInverse(nullptr, transform); }

    DirectX::XMMATRIX GetView() const { return View; }
    DirectX::XMMATRIX GetProjection() const { return Projection; }
    DirectX::XMVECTOR GetPositionVector() const { return Transform.r[3]; }

    void LookAtFloat3(DirectX::XMFLOAT3 position)
    {
        SetView(DirectX::XMMatrixLookAtLH(Transform.r[3], DirectX::XMVectorSet(position.x, position.y, position.z, 1), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1)));
    }

    void LookAtVector(DirectX::XMVECTOR position)
    {
        SetView(DirectX::XMMatrixLookAtLH(Transform.r[3], position, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1)));
    }

    void SetPositionFloat3(DirectX::XMFLOAT3 position)
    {
        DirectX::XMMATRIX transform = Transform;
        transform.r[3] = DirectX::XMVectorSet(position.x, position.y, position.z, 1);
        SetTransform(transform);
    }

    void SetPositionVector(DirectX::XMVECTOR position)
    {
        DirectX::XMMATRIX transform = Transform;
        transform.r[3] = position;
        SetTransform(transform);
    }

    void OnResize(uint32_t width, uint32_t height)
    {
        AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        Projection = DirectX::XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
    }
};
