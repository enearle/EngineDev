#pragma once
#include <DirectXMath.h>
#include "RHI/RHIStructures.h"


class Light
{
    bool CastsShadows = false;
    RHIStructures::LightData Data = {};
public:
    Light() = default;
    Light(const RHIStructures::LightData& data, bool castsShadows = false) : Data(data), CastsShadows(castsShadows) {}
    
    DirectX::XMFLOAT4X4 GetLightMatrix() const
    {
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(Data.Position.x, Data.Position.y, Data.Position.z, 1),
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), 
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(Data.Angle, 1.0f, 0.1f, Data.Radius);
        DirectX::XMFLOAT4X4 lightMatrix;
        DirectX::XMStoreFloat4x4(&lightMatrix, view * proj);
        
        return lightMatrix;
    }
};
