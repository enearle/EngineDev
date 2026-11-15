#include "SceneExtraction.h"

#include <algorithm>
#include <unordered_map>
#include "SceneNode.h"
#include "../Animation/AnimatorComponent.h"
#include "../Camera.h"
#include "../Light.h"
#include "Renderer.h"

using namespace DirectX;

// The first palette entry of each animator extracted this frame.
static std::unordered_map<const AnimatorComponent*, uint32_t> PaletteOffsets;

static uint32_t ExtractPalette(const AnimatorComponent* animator, RenderScene& scene)
{
    auto [found, isNew] = PaletteOffsets.try_emplace(animator, static_cast<uint32_t>(scene.BoneMatrices.size()));
    if (isNew)
        scene.BoneMatrices.insert(scene.BoneMatrices.end(), animator->GetPalette().begin(), animator->GetPalette().end());
    return found->second;
}

static void ExtractNode(SceneNode* node, RenderScene& scene)
{
    MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType));
    if (component)
    {
        MeshAsset* asset = component->GetMeshAsset();
        AnimatorComponent* animator = component->GetAnimator();

        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, node->GetWorldMatrix());
        XMFLOAT4X4 characterWorld = {};
        XMFLOAT4 characterBounds = {};
        if (animator)
        {
            XMMATRIX matrix = animator->GetOwner()->GetWorldMatrix();
            XMStoreFloat4x4(&characterWorld, matrix);
            const XMFLOAT4& model = animator->GetModelBounds();
            float scale = (std::max)({ XMVectorGetX(XMVector3Length(matrix.r[0])), XMVectorGetX(XMVector3Length(matrix.r[1])),
                                       XMVectorGetX(XMVector3Length(matrix.r[2])) });
            XMStoreFloat4(&characterBounds, XMVectorSetW(XMVector3TransformCoord(XMVectorSet(model.x, model.y, model.z, 1.0f), matrix), model.w * scale));
        }

        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
        {
            const Mesh& mesh = asset->GetMesh(i);
            if (!mesh.GetGPUMesh().IsValid())
                continue;

            RenderObject object;
            object.Mesh = mesh.GetGPUMesh();
            object.Material = component->GetSubmeshMaterial(i);
            object.World = world;
            // Skinned vertices are in model space, so they take the animator node's world matrix and
            // the bounds of its joints.
            if (mesh.IsSkinned() && animator)
            {
                object.World = characterWorld;
                object.Bounds = characterBounds;
                object.SkinnedMesh = component->GetSkinnedMesh(i);
                object.BoneOffset = ExtractPalette(animator, scene);
            }
            scene.Objects.push_back(object);
        }
    }

    for (SceneNode* child : node->GetChildren())
        ExtractNode(child, scene);
}

void SceneExtraction::ExtractFrame(const Camera* camera, const std::vector<const Light*>& lights)
{
    RenderScene& scene = Renderer::GetRenderScene();
    scene.Clear();
    PaletteOffsets.clear();

    for (SceneNode* root : SceneNode::GetRootNodes())
        ExtractNode(root, scene);

    if (camera)
    {
        XMStoreFloat4x4(&scene.Camera.View, camera->GetView());
        XMStoreFloat4x4(&scene.Camera.Projection, camera->GetProjection());
        XMStoreFloat3(&scene.Camera.Position, camera->GetPositionVector());
        scene.HasCamera = true;
    }

    for (const Light* light : lights)
    {
        RenderLight renderLight;
        renderLight.Data = light->GetData();
        renderLight.ViewProjection = light->GetLightMatrix();
        renderLight.CastsShadows = light->IsShadowCaster();
        scene.Lights.push_back(renderLight);
    }
}
