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
    
    DirectX::XMMATRIX anim0 = DirectX::XMMatrixIdentity();//DirectX::XMMatrixTranslation(xPos, 0, yPos);
    DirectX::XMMATRIX anim1 = DirectX::XMMatrixRotationZ(xRot * 0.5f);
    DirectX::XMMATRIX anim2 = DirectX::XMMatrixRotationZ(xRot * 0.25f);
    
    DirectX::XMMATRIX bindPose0 = Derp->GetBoneOffsets()[0];
    DirectX::XMMATRIX bindPose1 = Derp->GetBoneOffsets()[1];
    DirectX::XMMATRIX bindPose2 = Derp->GetBoneOffsets()[2];
    
    DirectX::XMMATRIX globalInverse0 = DirectX::XMMatrixInverse(nullptr, Derp->GetBoneTransforms()[0]);
    DirectX::XMMATRIX globalInverse1 = DirectX::XMMatrixInverse(nullptr, Derp->GetBoneTransforms()[1]);
    DirectX::XMMATRIX globalInverse2 = DirectX::XMMatrixInverse(nullptr, Derp->GetBoneTransforms()[2]);
    
    DirectX::XMMATRIX mat0 = globalInverse0 * anim0 * bindPose0;
    DirectX::XMMATRIX mat1 = globalInverse1 * mat0 * anim1 * bindPose1;
    DirectX::XMMATRIX mat2 = globalInverse2 * mat1 * anim2 * bindPose2;
    
    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[0], mat0);
    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[1], mat1);
    DirectX::XMStoreFloat4x4(&MappedDataAddr->BoneTransforms[2], mat2);
}
