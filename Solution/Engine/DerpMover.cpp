#include "DerpMover.h"
#include "TempGameObject.h"
#include "Common/RHI/BufferAllocator.h"

struct BoneData
{
    DirectX::XMFLOAT4X4 BoneTransforms[128];
};

void DerpMover::MoveTheDerp(double dt)
{
    BufferAllocation bufferAllocation = BufferAllocator::GetInstance()->GetBufferAllocation(Derp->GetBoneBufferID());
    if (!bufferAllocation.IsMapped || bufferAllocation.Address == nullptr)
        throw std::runtime_error("VP buffer is not mapped! Check MemoryAccess flags.");
            
    BoneData* MappedDataAddr = static_cast<BoneData*>(bufferAllocation.Address);
    
    ProgressMove += static_cast<float>(dt);
    while (ProgressMove > DurationMove)
        ProgressMove -= DurationMove;
    
    if (DanceForward)
    {
        ProgressDance += static_cast<float>(dt);
        if (ProgressDance > 1.0f)
        {
            DanceForward = false;
            ProgressDance = 1.0f;
        }
    }
    else
    {
        ProgressDance -= static_cast<float>(dt);
        if (ProgressDance < -1.0f)
        {
            DanceForward = true;
            ProgressDance = -1.0f;
        }
    }
    
    // I still need a way to hand bone offsets and hierarchy correctly, but this is a start 
    float xPos = cos(ProgressMove / DurationMove * DirectX::XM_2PI) * 0.1;
    float yPos = sin(ProgressMove / DurationMove * DirectX::XM_2PI) * 0.1;
    float xRot = ProgressDance * DirectX::XM_PI / 4;
    
    DirectX::XMFLOAT4X4 bone0;
    DirectX::XMFLOAT4X4 bone1;
    DirectX::XMFLOAT4X4 bone2;
    
    DirectX::XMMATRIX anim0 = DirectX::XMMatrixTranslation(xPos, yPos, 0);
    DirectX::XMMATRIX anim1 = DirectX::XMMatrixRotationY(xRot);
    DirectX::XMMATRIX anim2 = DirectX::XMMatrixRotationY(-xRot);
    
    DirectX::XMMATRIX mat0 = anim0;
    DirectX::XMMATRIX mat1 = anim1 * anim0 ;
    DirectX::XMMATRIX mat2 = anim2 * anim1 * anim0;

    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[0], mat0);
    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[1], mat1);
    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[2], mat2);
}
