#include "DerpMover.h"
#include <stdexcept>
#include "Scene/SceneNode.h"

using namespace DirectX;

static void CollectSkinnedComponents(SceneNode* node, std::vector<MeshComponent*>& out)
{
    MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType));
    if (component)
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
            if (asset->GetMesh(i).IsSkinned())
            {
                out.push_back(component);
                break;
            }
    }

    for (SceneNode* child : node->GetChildren())
        CollectSkinnedComponents(child, out);
}

DerpMover::DerpMover(SceneNode* derpRoot)
{
    CollectSkinnedComponents(derpRoot, SkinnedComponents);

    for (MeshComponent* component : SkinnedComponents)
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
        {
            const std::vector<XMMATRIX>& offsets = asset->GetMesh(i).GetBoneOffsets();
            if (offsets.size() > BoneOffsets.size())
                BoneOffsets.resize(offsets.size(), XMMatrixIdentity());
            for (size_t j = 0; j < offsets.size(); j++)
                BoneOffsets[j] = offsets[j];
        }
    }

    if (BoneOffsets.size() < 3)
        throw std::runtime_error("DerpMover needs a skinned mesh with at least three bones.");

    Palette.resize(BoneOffsets.size());
    for (XMFLOAT4X4& bone : Palette)
        XMStoreFloat4x4(&bone, XMMatrixIdentity());
    for (MeshComponent* component : SkinnedComponents)
        component->GetBonePalette() = Palette;
}

void DerpMover::MoveTheDerp(double dt)
{
    ProgressMove += static_cast<float>(dt);
    while (ProgressMove > DurationMove)
        ProgressMove -= DurationMove;

    ProgressDance += DanceForward ? static_cast<float>(dt) : -static_cast<float>(dt);
    if (ProgressDance > 1.0f)
    {
        DanceForward = false;
        ProgressDance = 1.0f;
    }
    else if (ProgressDance < -1.0f)
    {
        DanceForward = true;
        ProgressDance = -1.0f;
    }

    float xPos = cos(ProgressMove / DurationMove * XM_2PI) * 0.1f;
    float yPos = sin(ProgressMove / DurationMove * XM_2PI) * 0.1f;
    float xRot = ProgressDance * XM_PI / 4;

    XMMATRIX anim0 = XMMatrixTranslation(xPos, 0, yPos);
    XMMATRIX anim1 = XMMatrixRotationZ(xRot * 0.5f);
    XMMATRIX anim2 = XMMatrixRotationZ(xRot * 0.5f);

    XMMATRIX mat0 = XMMatrixInverse(nullptr, BoneOffsets[0]) * anim0 * BoneOffsets[0];
    XMMATRIX mat1 = mat0 * XMMatrixInverse(nullptr, BoneOffsets[1]) * anim1 * BoneOffsets[1];
    XMMATRIX mat2 = mat1 * XMMatrixInverse(nullptr, BoneOffsets[2]) * anim2 * BoneOffsets[2];

    XMStoreFloat4x4(&Palette[0], mat0);
    XMStoreFloat4x4(&Palette[1], mat1);
    XMStoreFloat4x4(&Palette[2], mat2);
    for (MeshComponent* component : SkinnedComponents)
        component->GetBonePalette() = Palette;
}
