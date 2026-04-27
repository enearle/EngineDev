#pragma once
#include <DirectXMath.h>

#include "RHI/BufferAllocator.h"
#include "RHI/RHIConstants.h"

struct VPData
{
    DirectX::XMFLOAT4X4 ViewProjection;
    DirectX::XMFLOAT4 CameraPosition;
    uint8_t _padding[176];
};

class Camera
{
    float FOV;
    float AspectRatio;
    DirectX::XMMATRIX Transform;
    DirectX::XMMATRIX View;
    DirectX::XMMATRIX Projection;
    VPData VPBufferData;
    bool IsVPDataDirty = true;
    
    void UpdateVPData()
    {
        DirectX::XMMATRIX vpMatrix = View * Projection;
        DirectX::XMFLOAT4X4 vp;
        DirectX::XMStoreFloat4x4(&vp, vpMatrix);
        DirectX::XMFLOAT4 position;
        DirectX::XMStoreFloat4(&position, Transform.r[3]);
        VPBufferData = VPData{.ViewProjection = vp, .CameraPosition = position };
    }
    
public:
    VPData& GetVpData()
    {
        if (IsVPDataDirty) UpdateVPData();
        return VPBufferData;
    }
    
private:
    
    // Should consider separating the Camera and it's rendering code
    // Could be needed for render targets like minimaps etc.
    static VPData ActiveVPData;
    static VPData* MappedDataAddr;
    static bool BufferInitialized;
    static uint64_t BufferID;
    static void InitializeBuffer(Camera* camera)
    {
        if (!BufferInitialized)
        {
            ActiveCamera = camera;
            BufferDesc VPBufferDesc = RHIConstants::DefaultConstantBufferDesc;
            VPBufferDesc.Size = 256;
            VPBufferDesc.Access = MemoryAccess(9);
            VPBufferDesc.InitialData = &ActiveVPData;
            BufferID = BufferAllocator::GetInstance()->CreateBuffer(VPBufferDesc);
            BufferAllocation vpAllocation = BufferAllocator::GetInstance()->GetBufferAllocation(BufferID);
            if (!vpAllocation.IsMapped || vpAllocation.Address == nullptr)
                throw std::runtime_error("VP buffer is not mapped! Check MemoryAccess flags.");
            
            MappedDataAddr = static_cast<VPData*>(vpAllocation.Address);
        }
    }
    /////////////////////////////////////////////////////////////////////
    
public:
    
    static Camera* ActiveCamera;
    void SetView(DirectX::XMMATRIX view) { View = view; Transform = DirectX::XMMatrixInverse(nullptr, view); IsVPDataDirty = true; }
    void SetTransform(DirectX::XMMATRIX transform) { Transform = transform; View = DirectX::XMMatrixInverse(nullptr, transform); IsVPDataDirty = true; }
    static uint64_t GetBufferID() { return BufferID; };
    
    static void UpdateMainCameraRenderData()
    {
        *MappedDataAddr = ActiveCamera->GetVpData();
    }
    
    Camera(float fovDeg, float aspectRatio, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation = {}, float nearPlane = 0.1f, float farPlane = 100.0f) : AspectRatio(aspectRatio)
    {
        InitializeBuffer(this);
        SetTransform(DirectX::XMMatrixTranslation(position.x, position.y, position.z) * DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z));
        FOV = fovDeg / 180 * DirectX::XM_PI;
        Projection = DirectX::XMMatrixPerspectiveFovLH(FOV, AspectRatio, nearPlane, farPlane);
    }
    
    Camera(float fovDeg, float aspectRatio, const DirectX::XMMATRIX& transformOrView, bool isView = false, float nearPlane = 0.1f, float farPlane = 1000.0f) : AspectRatio(aspectRatio)
    {
        InitializeBuffer(this);
        isView ? SetView(transformOrView) : SetTransform(transformOrView);
        FOV = fovDeg / 180 * DirectX::XM_PI;
        Projection = DirectX::XMMatrixPerspectiveFovLH(FOV, AspectRatio, nearPlane, farPlane);
    }
    
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
        DirectX::XMMATRIX transfrom = Transform;
        transfrom.r[3] = DirectX::XMVectorSet(position.x, position.y, position.z, 1);
        SetTransform(transfrom);
        
    }
    
    void SetPositionVector(DirectX::XMVECTOR position)
    {
        DirectX::XMMATRIX transfrom = Transform;
        transfrom.r[3] = position;
        SetTransform(transfrom);
    }
    
    DirectX::XMVECTOR GetPositionVector() const
    {
        return Transform.r[3];
    }

};
