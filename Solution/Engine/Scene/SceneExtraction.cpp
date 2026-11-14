#include "SceneExtraction.h"

#include "SceneNode.h"
#include "../Camera.h"
#include "../Light.h"
#include "Renderer.h"

using namespace DirectX;

static void ExtractNode(SceneNode* node, RenderScene& scene)
{
    MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType));
    if (component)
    {
        MeshAsset* asset = component->GetMeshAsset();

        uint32_t boneOffset = static_cast<uint32_t>(scene.BoneMatrices.size());
        const std::vector<XMFLOAT4X4>& palette = component->GetBonePalette();
        scene.BoneMatrices.insert(scene.BoneMatrices.end(), palette.begin(), palette.end());

        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, node->GetWorldMatrix());

        for (uint32_t i = 0; i < asset->GetMeshCount(); i++)
        {
            const Mesh& mesh = asset->GetMesh(i);
            if (!mesh.GetGPUMesh().IsValid())
                continue;

            RenderObject object;
            object.Mesh = mesh.GetGPUMesh();
            object.Material = component->GetSubmeshMaterial(i);
            object.World = world;
            object.BoneOffset = boneOffset;
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
