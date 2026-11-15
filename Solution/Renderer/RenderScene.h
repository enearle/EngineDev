#pragma once
#include <vector>
#include <DirectXMath.h>
#include "RHI/RHIStructures.h"

using MeshHandle = Handle<struct MeshHandleTag>;
using MaterialHandle = Handle<struct MaterialHandleTag>;
using SkinnedMeshHandle = Handle<struct SkinnedMeshHandleTag>;

enum class VertexLayout : uint8_t
{
    Static,
    Skinned,
    Count
};

struct RenderObject
{
    MeshHandle Mesh = {};
    MaterialHandle Material = {};
    DirectX::XMFLOAT4X4 World = {};
    // A skinned object is skinned this frame with the palette starting at BoneOffset in
    // RenderScene::BoneMatrices, then drawn from the skinning pool; Mesh is its source.
    SkinnedMeshHandle SkinnedMesh = {};
    uint32_t BoneOffset = 0;
    DirectX::XMFLOAT4 Bounds = {};      // World-space sphere (centre, radius) replacing the mesh's when the radius is above 0
    bool CastsShadows = true;
};

struct RenderLight
{
    RHIStructures::LightData Data = {};
    DirectX::XMFLOAT4X4 ViewProjection = {};
    bool CastsShadows = false;
};

struct RenderCamera
{
    DirectX::XMFLOAT4X4 View = {};
    DirectX::XMFLOAT4X4 Projection = {};
    DirectX::XMFLOAT3 Position = {};
};

// What the renderer draws this frame. The engine fills it once per frame before DrawFrame; nothing
// else submits draws.
struct RenderScene
{
    std::vector<RenderObject> Objects;
    std::vector<DirectX::XMFLOAT4X4> BoneMatrices;
    std::vector<RenderLight> Lights;
    RenderCamera Camera = {};
    bool HasCamera = false;

    void Clear()
    {
        Objects.clear();
        BoneMatrices.clear();
        Lights.clear();
        HasCamera = false;
    }
};

// Mirrors FrameConstants in Shaders/Common.hlsli.
constexpr uint32_t MaxLights = 4;
struct FrameConstants
{
    DirectX::XMFLOAT4X4 ViewProjection;
    DirectX::XMFLOAT4X4 InverseViewProjection;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxLights];
    RHIStructures::LightData Lights[MaxLights];
};

// Mirrors InstanceData in Shaders/Common.hlsli.
struct InstanceData
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 Normal;
};
static_assert(sizeof(InstanceData) == 128);

// Mirrors DrawConstants in Shaders/Common.hlsli.
struct DrawConstants
{
    uint32_t FirstInstance;
    uint32_t ViewIndex;         // Which light a shadow draw renders for
    uint32_t Padding[2];
};
